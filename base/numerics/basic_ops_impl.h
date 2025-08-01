// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MINI_CHROMIUM_BASE_NUMERICS_BASIC_OPS_IMPL_H_
#define MINI_CHROMIUM_BASE_NUMERICS_BASIC_OPS_IMPL_H_

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace base::numerics::internal {

#if (defined(__GNUC__) && __GNUC__ >= 9) || defined(__clang__)
inline constexpr bool cxx17_is_constant_evaluated() noexcept {
  return __builtin_is_constant_evaluated();
}
#elif defined(_MSC_VER)
inline constexpr bool cxx17_is_constant_evaluated() noexcept {
  return false;
}
#else
inline constexpr bool cxx17_is_constant_evaluated() noexcept {
  return false;
}
#endif

// The correct type to perform math operations on given values of type `T`. This
// may be a larger type than `T` to avoid promotion to `int` which involves sign
// conversion!
template <class T, std::enable_if_t<std::is_integral_v<T>, int> = 0>
using MathType = std::conditional_t<
    sizeof(T) >= sizeof(int),
    T,
    std::conditional_t<std::is_signed_v<T>, int, unsigned int>>;

// Reverses the byte order of the integer.
template <
    class T,
    std::enable_if_t<std::is_unsigned_v<T> && std::is_integral_v<T>, int> = 0>
inline constexpr T SwapBytes(T value) {
  // MSVC intrinsics are not constexpr, so we provide our own constexpr
  // implementation. We provide it unconditionally so we can test it on all
  // platforms for correctness.
  if (cxx17_is_constant_evaluated()) {
    if constexpr (sizeof(T) == 1u) {
      return value;
    } else if constexpr (sizeof(T) == 2u) {
      MathType<T> a = (MathType<T>(value) >> 0) & MathType<T>{0xff};
      MathType<T> b = (MathType<T>(value) >> 8) & MathType<T>{0xff};
      return static_cast<T>((a << 8) | (b << 0));
    } else if constexpr (sizeof(T) == 4u) {
      T a = (value >> 0) & T{0xff};
      T b = (value >> 8) & T{0xff};
      T c = (value >> 16) & T{0xff};
      T d = (value >> 24) & T{0xff};
      return (a << 24) | (b << 16) | (c << 8) | (d << 0);
    } else {
      static_assert(sizeof(T) == 8u);
      T a = (value >> 0) & T{0xff};
      T b = (value >> 8) & T{0xff};
      T c = (value >> 16) & T{0xff};
      T d = (value >> 24) & T{0xff};
      T e = (value >> 32) & T{0xff};
      T f = (value >> 40) & T{0xff};
      T g = (value >> 48) & T{0xff};
      T h = (value >> 56) & T{0xff};
      return (a << 56) | (b << 48) | (c << 40) | (d << 32) |  //
             (e << 24) | (f << 16) | (g << 8) | (h << 0);
    }
  }

#if _MSC_VER
  if constexpr (sizeof(T) == 1u) {
    return value;
  } else if constexpr (sizeof(T) == sizeof(unsigned short)) {
    using U = unsigned short;
    return _byteswap_ushort(U{value});
  } else if constexpr (sizeof(T) == sizeof(unsigned long)) {
    using U = unsigned long;
    return _byteswap_ulong(U{value});
  } else {
    static_assert(sizeof(T) == 8u);
    return _byteswap_uint64(value);
  }
#else
  if constexpr (sizeof(T) == 1u) {
    return value;
  } else if constexpr (sizeof(T) == 2u) {
    return __builtin_bswap16(uint16_t{value});
  } else if constexpr (sizeof(T) == 4u) {
    return __builtin_bswap32(value);
  } else {
    static_assert(sizeof(T) == 8u);
    return __builtin_bswap64(value);
  }
#endif
}

// Signed values are byte-swapped as unsigned values.
template <
    class T,
    std::enable_if_t<std::is_signed_v<T> && std::is_integral_v<T>, int> = 0>
inline constexpr T SwapBytes(T value) {
  return static_cast<T>(SwapBytes(static_cast<std::make_unsigned_t<T>>(value)));
}

// Converts from a byte array to an integer.
template <
    class T,
    std::enable_if_t<std::is_unsigned_v<T> && std::is_integral_v<T>, int> = 0>
inline constexpr T FromLittleEndian(span<const uint8_t, sizeof(T)> bytes) {
  T val;
  if (cxx17_is_constant_evaluated()) {
    val = T{0};
    for (size_t i = 0u; i < sizeof(T); i += 1u) {
      // SAFETY: `i < sizeof(T)` (the number of bytes in T), so `(8 * i)` is
      // less than the number of bits in T.
      val |= MathType<T>(bytes[i]) << (8u * i);
    }
  } else {
    // SAFETY: `bytes` has sizeof(T) bytes, and `val` is of type `T` so has
    // sizeof(T) bytes, and the two can not alias as `val` is a stack variable.
    memcpy(&val, bytes.data(), sizeof(T));
  }
  return val;
}

// Converts to a byte array from an integer.
template <
    class T,
    std::enable_if_t<std::is_unsigned_v<T> && std::is_integral_v<T>, int> = 0>
inline constexpr std::array<uint8_t, sizeof(T)> ToLittleEndian(T val) {
  auto bytes = std::array<uint8_t, sizeof(T)>();
  if (cxx17_is_constant_evaluated()) {
    for (size_t i = 0u; i < sizeof(T); i += 1u) {
      const auto last_byte = static_cast<uint8_t>(val & 0xff);
      // The low bytes go to the front of the array in little endian.
      bytes[i] = last_byte;
      // If `val` is one byte, this shift would be UB. But it's also not needed
      // since the loop will not run again.
      if constexpr (sizeof(T) > 1u) {
        val >>= 8u;
      }
    }
  } else {
    // SAFETY: `bytes` has sizeof(T) bytes, and `val` is of type `T` so has
    // sizeof(T) bytes, and the two can not alias as `val` is a stack variable.
    memcpy(bytes.data(), &val, sizeof(T));
  }
  return bytes;
}

}  // namespace base::numerics::internal

#endif  //  MINI_CHROMIUM_BASE_NUMERICS_BASIC_OPS_IMPL_H_
