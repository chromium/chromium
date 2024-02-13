// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_BYTE_CONVERSIONS_H_
#define BASE_NUMERICS_BYTE_CONVERSIONS_H_

#include <bit>
#include <cstdint>
#include <cstring>
#include <span>
#include <type_traits>

#include "base/numerics/basic_ops_impl.h"

namespace base::numerics {

// Returns a value with all bytes in |x| swapped, i.e. reverses the endianness.
// TODO(pkasting): Once C++23 is available, replace with std::byteswap.
template <class T>
  requires(std::is_unsigned_v<T> && std::is_integral_v<T>)
inline constexpr T ByteSwap(T value) {
  return numerics::internal::SwapBytes(value);
}

// Returns a uint8_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
//
// Note that since a single byte can have only one ordering, this just copies
// the byte out of the span. This provides a consistent function for the
// operation nonetheless.
inline constexpr uint8_t U8FromNativeEndian(
    std::span<const uint8_t, 1u> bytes) {
  return bytes[0];
}
// Returns a uint16_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
inline constexpr uint16_t U16FromNativeEndian(
    std::span<const uint8_t, 2u> bytes) {
  return internal::FromLittleEndian<uint16_t>(bytes);
}
// Returns a uint32_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
inline constexpr uint32_t U32FromNativeEndian(
    std::span<const uint8_t, 4u> bytes) {
  return internal::FromLittleEndian<uint32_t>(bytes);
}
// Returns a uint64_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
inline constexpr uint64_t U64FromNativeEndian(
    std::span<const uint8_t, 8u> bytes) {
  return internal::FromLittleEndian<uint64_t>(bytes);
}
// Returns a float with the value in `bytes` interpreted as the native endian
// encoding of the number for the machine.
//
// This is suitable for decoding numbers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
inline constexpr float FloatFromNativeEndian(
    std::span<const uint8_t, 4u> bytes) {
  return std::bit_cast<float>(U32FromNativeEndian(bytes));
}
// Returns a double with the value in `bytes` interpreted as the native endian
// encoding of the number for the machine.
//
// This is suitable for decoding numbers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
inline constexpr double DoubleFromNativeEndian(
    std::span<const uint8_t, 8u> bytes) {
  return std::bit_cast<double>(U64FromNativeEndian(bytes));
}

// Returns a uint8_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
//
// Note that since a single byte can have only one ordering, this just copies
// the byte out of the span. This provides a consistent function for the
// operation nonetheless.
inline constexpr uint8_t U8FromLittleEndian(
    std::span<const uint8_t, 1u> bytes) {
  return bytes[0];
}
// Returns a uint16_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
inline constexpr uint16_t U16FromLittleEndian(
    std::span<const uint8_t, 2u> bytes) {
  return internal::FromLittleEndian<uint16_t>(bytes);
}
// Returns a uint32_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
inline constexpr uint32_t U32FromLittleEndian(
    std::span<const uint8_t, 4u> bytes) {
  return internal::FromLittleEndian<uint32_t>(bytes);
}
// Returns a uint64_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
inline constexpr uint64_t U64FromLittleEndian(
    std::span<const uint8_t, 8u> bytes) {
  return internal::FromLittleEndian<uint64_t>(bytes);
}
// Returns a float with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding numbers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
inline constexpr float FloatFromLittleEndian(
    std::span<const uint8_t, 4u> bytes) {
  return std::bit_cast<float>(U32FromLittleEndian(bytes));
}
// Returns a double with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding numbers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
inline constexpr double DoubleFromLittleEndian(
    std::span<const uint8_t, 8u> bytes) {
  return std::bit_cast<double>(U64FromLittleEndian(bytes));
}

// Returns a uint8_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
//
// Note that since a single byte can have only one ordering, this just copies
// the byte out of the span. This provides a consistent function for the
// operation nonetheless.
inline constexpr uint8_t U8FromBigEndian(std::span<const uint8_t, 1u> bytes) {
  return bytes[0];
}
// Returns a uint16_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
inline constexpr uint16_t U16FromBigEndian(std::span<const uint8_t, 2u> bytes) {
  return ByteSwap(internal::FromLittleEndian<uint16_t>(bytes));
}
// Returns a uint32_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
inline constexpr uint32_t U32FromBigEndian(std::span<const uint8_t, 4u> bytes) {
  return ByteSwap(internal::FromLittleEndian<uint32_t>(bytes));
}
// Returns a uint64_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
inline constexpr uint64_t U64FromBigEndian(std::span<const uint8_t, 8u> bytes) {
  return ByteSwap(internal::FromLittleEndian<uint64_t>(bytes));
}
// Returns a float with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding numbers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
inline constexpr float FloatFromBigEndian(std::span<const uint8_t, 4u> bytes) {
  return std::bit_cast<float>(U32FromBigEndian(bytes));
}
// Returns a double with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding numbers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
inline constexpr double DoubleFromBigEndian(
    std::span<const uint8_t, 8u> bytes) {
  return std::bit_cast<double>(U64FromBigEndian(bytes));
}

}  // namespace base::numerics

#endif  //  BASE_NUMERICS_BYTE_CONVERSIONS_H_
