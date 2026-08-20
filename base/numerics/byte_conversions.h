// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_NUMERICS_BYTE_CONVERSIONS_H_
#define BASE_NUMERICS_BYTE_CONVERSIONS_H_

#include <array>
#include <bit>
#include <cstdint>
#include <span>
#include <type_traits>

#include "base/numerics/basic_ops_impl.h"

// Chromium only builds and runs on Little Endian machines.
static_assert(std::endian::native == std::endian::little);

namespace base {

// A unified concept for all scalar types suitable for safe, bitwise endianness
// transformations and byte serialization without padding-bit hazards or
// undefined object representations.
template <typename T>
concept ByteConvertible =
    numerics_internal::PackedIntegral<T> ||
    numerics_internal::IeeeBinaryFloat<T> || numerics_internal::PackedEnum<T>;

// Returns a byte convertible type with the value in `bytes` interpreted as a
// little-endian encoding of the type.
//
// This is suitable for decoding types encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
template <typename T>
  requires(ByteConvertible<T>)
constexpr T FromLittleEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  return numerics_internal::FromEndian<std::endian::little, T>(bytes);
}

// Returns a byte convertible type with the value in `bytes` interpreted as a
// big-endian encoding of the type.
//
// This is suitable for decoding types encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
template <typename T>
  requires(ByteConvertible<T>)
constexpr T FromBigEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  return numerics_internal::FromEndian<std::endian::big, T>(bytes);
}

// Returns a byte convertible type with the value in `bytes` interpreted as the
// native endian encoding of the integer for the machine.
//
// This is suitable for decoding types that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
template <typename T>
  requires(ByteConvertible<T>)
constexpr T FromNativeEndian(std::span<const uint8_t, sizeof(T)> bytes) {
  return numerics_internal::FromEndian<std::endian::native, T>(bytes);
}

// Returns a byte array holding the value of a byte convertible type encoded as
// the little-endian encoding of the type.
//
// This is suitable for encoding types explicitly in little endian, which is
// a good practice with storing and reading data from storage. Use the
// native-endian versions when working with values that will always be in
// memory, such as when stored in shared-memory (or passed through IPC) as a
// byte buffer.
//
// NOTE: To prevent surprising bugs from C++ integer promotion rules (e.g.,
// `uint8_t{1} << 5` evaluating to a 32-bit `int`), template argument deduction
// is disabled. Callers must explicitly specify the type `T` (e.g.,
// `ToLittleEndian<uint8_t>(val)`), or use one of the explicitly sized
// helpers (e.g., `U8ToLittleEndian(val)`).
template <typename T>
  requires(ByteConvertible<T>)
constexpr std::array<uint8_t, sizeof(T)> ToLittleEndian(
    std::type_identity_t<T> val) {
  return numerics_internal::ToEndian<std::endian::little>(val);
}

// Returns a byte array holding the value of a byte convertible type encoded as
// the big-endian encoding of the type.
//
// This is suitable for encoding types explicitly in big endian, such as for
// network order. Use the native-endian versions when working with values that
// are always in memory, such as when stored in shared-memory (or passed through
// IPC) as a byte buffer. Use the little-endian encoding for storing and reading
// from storage.
//
// NOTE: To prevent surprising bugs from C++ integer promotion rules (e.g.,
// `uint8_t{1} << 5` evaluating to a 32-bit `int`), template argument deduction
// is disabled. Callers must explicitly specify the type `T` (e.g.,
// `ToBigEndian<uint8_t>(val)`), or use one of the explicitly sized
// helpers (e.g., `U8ToBigEndian(val)`).
template <typename T>
  requires(ByteConvertible<T>)
constexpr std::array<uint8_t, sizeof(T)> ToBigEndian(
    std::type_identity_t<T> val) {
  return numerics_internal::ToEndian<std::endian::big>(val);
}

// Returns a byte array holding the value of a byte convertible type encoded as
// the native endian encoding of the type for the machine.
//
// This is suitable for encoding types that will always be kept in native
// encoding, such as for storing in shared-memory (or sending through IPC) as a
// byte buffer. Prefer an explicit little endian when storing data into external
// storage, and explicit big endian for network order.
//
// NOTE: To prevent surprising bugs from C++ integer promotion rules (e.g.,
// `uint8_t{1} << 5` evaluating to a 32-bit `int`), template argument deduction
// is disabled. Callers must explicitly specify the type `T` (e.g.,
// `ToNativeEndian<uint8_t>(val)`), or use one of the explicitly sized
// helpers (e.g., `U8ToNativeEndian(val)`).
template <typename T>
  requires(ByteConvertible<T>)
constexpr std::array<uint8_t, sizeof(T)> ToNativeEndian(
    std::type_identity_t<T> val) {
  return numerics_internal::ToEndian<std::endian::native>(val);
}

// Returns a uint8_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
constexpr uint8_t U8FromNativeEndian(std::span<const uint8_t, 1u> bytes) {
  return FromNativeEndian<uint8_t>(bytes);
}
// Returns a uint16_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
constexpr uint16_t U16FromNativeEndian(std::span<const uint8_t, 2u> bytes) {
  return FromNativeEndian<uint16_t>(bytes);
}
// Returns a uint32_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
constexpr uint32_t U32FromNativeEndian(std::span<const uint8_t, 4u> bytes) {
  return FromNativeEndian<uint32_t>(bytes);
}
// Returns a uint64_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
constexpr uint64_t U64FromNativeEndian(std::span<const uint8_t, 8u> bytes) {
  return FromNativeEndian<uint64_t>(bytes);
}
// Returns a int8_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
constexpr int8_t I8FromNativeEndian(std::span<const uint8_t, 1u> bytes) {
  return FromNativeEndian<int8_t>(bytes);
}
// Returns a int16_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
constexpr int16_t I16FromNativeEndian(std::span<const uint8_t, 2u> bytes) {
  return FromNativeEndian<int16_t>(bytes);
}
// Returns a int32_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
constexpr int32_t I32FromNativeEndian(std::span<const uint8_t, 4u> bytes) {
  return FromNativeEndian<int32_t>(bytes);
}
// Returns a int64_t with the value in `bytes` interpreted as the native endian
// encoding of the integer for the machine.
//
// This is suitable for decoding integers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
constexpr int64_t I64FromNativeEndian(std::span<const uint8_t, 8u> bytes) {
  return FromNativeEndian<int64_t>(bytes);
}

// Returns a float with the value in `bytes` interpreted as the native endian
// encoding of the number for the machine.
//
// This is suitable for decoding numbers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
constexpr float FloatFromNativeEndian(std::span<const uint8_t, 4u> bytes) {
  return FromNativeEndian<float>(bytes);
}
// Returns a double with the value in `bytes` interpreted as the native endian
// encoding of the number for the machine.
//
// This is suitable for decoding numbers that were always kept in native
// encoding, such as when stored in shared-memory (or through IPC) as a byte
// buffer. Prefer an explicit little endian when storing and reading data from
// storage, and explicit big endian for network order.
constexpr double DoubleFromNativeEndian(std::span<const uint8_t, 8u> bytes) {
  return FromNativeEndian<double>(bytes);
}

// Returns a uint8_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
constexpr uint8_t U8FromLittleEndian(std::span<const uint8_t, 1u> bytes) {
  return FromLittleEndian<uint8_t>(bytes);
}
// Returns a uint16_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
constexpr uint16_t U16FromLittleEndian(std::span<const uint8_t, 2u> bytes) {
  return FromLittleEndian<uint16_t>(bytes);
}
// Returns a uint32_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
constexpr uint32_t U32FromLittleEndian(std::span<const uint8_t, 4u> bytes) {
  return FromLittleEndian<uint32_t>(bytes);
}
// Returns a uint64_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
constexpr uint64_t U64FromLittleEndian(std::span<const uint8_t, 8u> bytes) {
  return FromLittleEndian<uint64_t>(bytes);
}
// Returns a int8_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
constexpr int8_t I8FromLittleEndian(std::span<const uint8_t, 1u> bytes) {
  return FromLittleEndian<int8_t>(bytes);
}
// Returns a int16_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
constexpr int16_t I16FromLittleEndian(std::span<const uint8_t, 2u> bytes) {
  return FromLittleEndian<int16_t>(bytes);
}
// Returns a int32_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
constexpr int32_t I32FromLittleEndian(std::span<const uint8_t, 4u> bytes) {
  return FromLittleEndian<int32_t>(bytes);
}
// Returns a int64_t with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
constexpr int64_t I64FromLittleEndian(std::span<const uint8_t, 8u> bytes) {
  return FromLittleEndian<int64_t>(bytes);
}
// Returns a float with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding numbers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
constexpr float FloatFromLittleEndian(std::span<const uint8_t, 4u> bytes) {
  return FromLittleEndian<float>(bytes);
}
// Returns a double with the value in `bytes` interpreted as a little-endian
// encoding of the integer.
//
// This is suitable for decoding numbers encoded explicitly in little endian,
// which is a good practice with storing and reading data from storage. Use
// the native-endian versions when working with values that were always in
// memory, such as when stored in shared-memory (or through IPC) as a byte
// buffer.
constexpr double DoubleFromLittleEndian(std::span<const uint8_t, 8u> bytes) {
  return FromLittleEndian<double>(bytes);
}

// Returns a uint8_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
constexpr uint8_t U8FromBigEndian(std::span<const uint8_t, 1u> bytes) {
  return FromBigEndian<uint8_t>(bytes);
}
// Returns a uint16_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
constexpr uint16_t U16FromBigEndian(std::span<const uint8_t, 2u> bytes) {
  return FromBigEndian<uint16_t>(bytes);
}
// Returns a uint32_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
constexpr uint32_t U32FromBigEndian(std::span<const uint8_t, 4u> bytes) {
  return FromBigEndian<uint32_t>(bytes);
}
// Returns a uint64_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
constexpr uint64_t U64FromBigEndian(std::span<const uint8_t, 8u> bytes) {
  return FromBigEndian<uint64_t>(bytes);
}
// Returns a int8_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
constexpr int8_t I8FromBigEndian(std::span<const uint8_t, 1u> bytes) {
  return FromBigEndian<int8_t>(bytes);
}
// Returns a int16_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
constexpr int16_t I16FromBigEndian(std::span<const uint8_t, 2u> bytes) {
  return FromBigEndian<int16_t>(bytes);
}
// Returns a int32_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
constexpr int32_t I32FromBigEndian(std::span<const uint8_t, 4u> bytes) {
  return FromBigEndian<int32_t>(bytes);
}
// Returns a int64_t with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding integers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
constexpr int64_t I64FromBigEndian(std::span<const uint8_t, 8u> bytes) {
  return FromBigEndian<int64_t>(bytes);
}
// Returns a float with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding numbers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
constexpr float FloatFromBigEndian(std::span<const uint8_t, 4u> bytes) {
  return FromBigEndian<float>(bytes);
}
// Returns a double with the value in `bytes` interpreted as a big-endian
// encoding of the integer.
//
// This is suitable for decoding numbers encoded explicitly in big endian, such
// as for network order. Use the native-endian versions when working with values
// that were always in memory, such as when stored in shared-memory (or through
// IPC) as a byte buffer.
constexpr double DoubleFromBigEndian(std::span<const uint8_t, 8u> bytes) {
  return FromBigEndian<double>(bytes);
}

// Returns a byte array holding the value of a uint8_t encoded as the native
// endian encoding of the integer for the machine.
//
// This is suitable for encoding integers that will always be kept in native
// encoding, such as for storing in shared-memory (or sending through IPC) as a
// byte buffer. Prefer an explicit little endian when storing data into external
// storage, and explicit big endian for network order.
constexpr std::array<uint8_t, 1u> U8ToNativeEndian(uint8_t val) {
  return ToNativeEndian<uint8_t>(val);
}
// Returns a byte array holding the value of a uint16_t encoded as the native
// endian encoding of the integer for the machine.
//
// This is suitable for encoding integers that will always be kept in native
// encoding, such as for storing in shared-memory (or sending through IPC) as a
// byte buffer. Prefer an explicit little endian when storing data into external
// storage, and explicit big endian for network order.
constexpr std::array<uint8_t, 2u> U16ToNativeEndian(uint16_t val) {
  return ToNativeEndian<uint16_t>(val);
}
// Returns a byte array holding the value of a uint32_t encoded as the native
// endian encoding of the integer for the machine.
//
// This is suitable for encoding integers that will always be kept in native
// encoding, such as for storing in shared-memory (or sending through IPC) as a
// byte buffer. Prefer an explicit little endian when storing data into external
// storage, and explicit big endian for network order.
constexpr std::array<uint8_t, 4u> U32ToNativeEndian(uint32_t val) {
  return ToNativeEndian<uint32_t>(val);
}
// Returns a byte array holding the value of a uint64_t encoded as the native
// endian encoding of the integer for the machine.
//
// This is suitable for encoding integers that will always be kept in native
// encoding, such as for storing in shared-memory (or sending through IPC) as a
// byte buffer. Prefer an explicit little endian when storing data into external
// storage, and explicit big endian for network order.
constexpr std::array<uint8_t, 8u> U64ToNativeEndian(uint64_t val) {
  return ToNativeEndian<uint64_t>(val);
}
// Returns a byte array holding the value of a int8_t encoded as the native
// endian encoding of the integer for the machine.
//
// This is suitable for encoding integers that will always be kept in native
// encoding, such as for storing in shared-memory (or sending through IPC) as a
// byte buffer. Prefer an explicit little endian when storing data into external
// storage, and explicit big endian for network order.
constexpr std::array<uint8_t, 1u> I8ToNativeEndian(int8_t val) {
  return ToNativeEndian<int8_t>(val);
}
// Returns a byte array holding the value of a int16_t encoded as the native
// endian encoding of the integer for the machine.
//
// This is suitable for encoding integers that will always be kept in native
// encoding, such as for storing in shared-memory (or sending through IPC) as a
// byte buffer. Prefer an explicit little endian when storing data into external
// storage, and explicit big endian for network order.
constexpr std::array<uint8_t, 2u> I16ToNativeEndian(int16_t val) {
  return ToNativeEndian<int16_t>(val);
}
// Returns a byte array holding the value of a int32_t encoded as the native
// endian encoding of the integer for the machine.
//
// This is suitable for encoding integers that will always be kept in native
// encoding, such as for storing in shared-memory (or sending through IPC) as a
// byte buffer. Prefer an explicit little endian when storing data into external
// storage, and explicit big endian for network order.
constexpr std::array<uint8_t, 4u> I32ToNativeEndian(int32_t val) {
  return ToNativeEndian<int32_t>(val);
}
// Returns a byte array holding the value of a int64_t encoded as the native
// endian encoding of the integer for the machine.
//
// This is suitable for encoding integers that will always be kept in native
// encoding, such as for storing in shared-memory (or sending through IPC) as a
// byte buffer. Prefer an explicit little endian when storing data into external
// storage, and explicit big endian for network order.
constexpr std::array<uint8_t, 8u> I64ToNativeEndian(int64_t val) {
  return ToNativeEndian<int64_t>(val);
}
// Returns a byte array holding the value of a float encoded as the native
// endian encoding of the number for the machine.
//
// This is suitable for encoding numbers that will always be kept in native
// encoding, such as for storing in shared-memory (or sending through IPC) as a
// byte buffer. Prefer an explicit little endian when storing data into external
// storage, and explicit big endian for network order.
constexpr std::array<uint8_t, 4u> FloatToNativeEndian(float val) {
  return ToNativeEndian<float>(val);
}
// Returns a byte array holding the value of a double encoded as the native
// endian encoding of the number for the machine.
//
// This is suitable for encoding numbers that will always be kept in native
// encoding, such as for storing in shared-memory (or sending through IPC) as a
// byte buffer. Prefer an explicit little endian when storing data into external
// storage, and explicit big endian for network order.
constexpr std::array<uint8_t, 8u> DoubleToNativeEndian(double val) {
  return ToNativeEndian<double>(val);
}

// Returns a byte array holding the value of a uint8_t encoded as the
// little-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in little endian, which is
// a good practice with storing and reading data from storage. Use the
// native-endian versions when working with values that will always be in
// memory, such as when stored in shared-memory (or passed through IPC) as a
// byte buffer.
constexpr std::array<uint8_t, 1u> U8ToLittleEndian(uint8_t val) {
  return ToLittleEndian<uint8_t>(val);
}
// Returns a byte array holding the value of a uint16_t encoded as the
// little-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in little endian, which is
// a good practice with storing and reading data from storage. Use the
// native-endian versions when working with values that will always be in
// memory, such as when stored in shared-memory (or passed through IPC) as a
// byte buffer.
constexpr std::array<uint8_t, 2u> U16ToLittleEndian(uint16_t val) {
  return ToLittleEndian<uint16_t>(val);
}
// Returns a byte array holding the value of a uint32_t encoded as the
// little-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in little endian, which is
// a good practice with storing and reading data from storage. Use the
// native-endian versions when working with values that will always be in
// memory, such as when stored in shared-memory (or passed through IPC) as a
// byte buffer.
constexpr std::array<uint8_t, 4u> U32ToLittleEndian(uint32_t val) {
  return ToLittleEndian<uint32_t>(val);
}
// Returns a byte array holding the value of a uint64_t encoded as the
// little-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in little endian, which is
// a good practice with storing and reading data from storage. Use the
// native-endian versions when working with values that will always be in
// memory, such as when stored in shared-memory (or passed through IPC) as a
// byte buffer.
constexpr std::array<uint8_t, 8u> U64ToLittleEndian(uint64_t val) {
  return ToLittleEndian<uint64_t>(val);
}
// Returns a byte array holding the value of a int8_t encoded as the
// little-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in little endian, which is
// a good practice with storing and reading data from storage. Use the
// native-endian versions when working with values that will always be in
// memory, such as when stored in shared-memory (or passed through IPC) as a
// byte buffer.
constexpr std::array<uint8_t, 1u> I8ToLittleEndian(int8_t val) {
  return ToLittleEndian<int8_t>(val);
}
// Returns a byte array holding the value of a int16_t encoded as the
// little-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in little endian, which is
// a good practice with storing and reading data from storage. Use the
// native-endian versions when working with values that will always be in
// memory, such as when stored in shared-memory (or passed through IPC) as a
// byte buffer.
constexpr std::array<uint8_t, 2u> I16ToLittleEndian(int16_t val) {
  return ToLittleEndian<int16_t>(val);
}
// Returns a byte array holding the value of a int32_t encoded as the
// little-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in little endian, which is
// a good practice with storing and reading data from storage. Use the
// native-endian versions when working with values that will always be in
// memory, such as when stored in shared-memory (or passed through IPC) as a
// byte buffer.
constexpr std::array<uint8_t, 4u> I32ToLittleEndian(int32_t val) {
  return ToLittleEndian<int32_t>(val);
}
// Returns a byte array holding the value of a int64_t encoded as the
// little-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in little endian, which is
// a good practice with storing and reading data from storage. Use the
// native-endian versions when working with values that will always be in
// memory, such as when stored in shared-memory (or passed through IPC) as a
// byte buffer.
constexpr std::array<uint8_t, 8u> I64ToLittleEndian(int64_t val) {
  return ToLittleEndian<int64_t>(val);
}
// Returns a byte array holding the value of a float encoded as the
// little-endian encoding of the number.
//
// This is suitable for encoding numbers explicitly in little endian, which is
// a good practice with storing and reading data from storage. Use the
// native-endian versions when working with values that will always be in
// memory, such as when stored in shared-memory (or passed through IPC) as a
// byte buffer.
constexpr std::array<uint8_t, 4u> FloatToLittleEndian(float val) {
  return ToLittleEndian<float>(val);
}
// Returns a byte array holding the value of a double encoded as the
// little-endian encoding of the number.
//
// This is suitable for encoding numbers explicitly in little endian, which is
// a good practice with storing and reading data from storage. Use the
// native-endian versions when working with values that will always be in
// memory, such as when stored in shared-memory (or passed through IPC) as a
// byte buffer.
constexpr std::array<uint8_t, 8u> DoubleToLittleEndian(double val) {
  return ToLittleEndian<double>(val);
}

// Returns a byte array holding the value of a uint8_t encoded as the big-endian
// encoding of the integer.
//
// This is suitable for encoding integers explicitly in big endian, such as for
// network order. Use the native-endian versions when working with values that
// are always in memory, such as when stored in shared-memory (or passed through
// IPC) as a byte buffer. Use the little-endian encoding for storing and reading
// from storage.
constexpr std::array<uint8_t, 1u> U8ToBigEndian(uint8_t val) {
  return ToBigEndian<uint8_t>(val);
}
// Returns a byte array holding the value of a uint16_t encoded as the
// big-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in big endian, such as for
// network order. Use the native-endian versions when working with values that
// are always in memory, such as when stored in shared-memory (or passed through
// IPC) as a byte buffer. Use the little-endian encoding for storing and reading
// from storage.
constexpr std::array<uint8_t, 2u> U16ToBigEndian(uint16_t val) {
  return ToBigEndian<uint16_t>(val);
}
// Returns a byte array holding the value of a uint32_t encoded as the
// big-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in big endian, such as for
// network order. Use the native-endian versions when working with values that
// are always in memory, such as when stored in shared-memory (or passed through
// IPC) as a byte buffer. Use the little-endian encoding for storing and reading
// from storage.
constexpr std::array<uint8_t, 4u> U32ToBigEndian(uint32_t val) {
  return ToBigEndian<uint32_t>(val);
}
// Returns a byte array holding the value of a uint64_t encoded as the
// big-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in big endian, such as for
// network order. Use the native-endian versions when working with values that
// are always in memory, such as when stored in shared-memory (or passed through
// IPC) as a byte buffer. Use the little-endian encoding for storing and reading
// from storage.
constexpr std::array<uint8_t, 8u> U64ToBigEndian(uint64_t val) {
  return ToBigEndian<uint64_t>(val);
}
// Returns a byte array holding the value of a int8_t encoded as the big-endian
// encoding of the integer.
//
// This is suitable for encoding integers explicitly in big endian, such as for
// network order. Use the native-endian versions when working with values that
// are always in memory, such as when stored in shared-memory (or passed through
// IPC) as a byte buffer. Use the little-endian encoding for storing and reading
// from storage.
constexpr std::array<uint8_t, 1u> I8ToBigEndian(int8_t val) {
  return ToBigEndian<int8_t>(val);
}
// Returns a byte array holding the value of a int16_t encoded as the
// big-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in big endian, such as for
// network order. Use the native-endian versions when working with values that
// are always in memory, such as when stored in shared-memory (or passed through
// IPC) as a byte buffer. Use the little-endian encoding for storing and reading
// from storage.
constexpr std::array<uint8_t, 2u> I16ToBigEndian(int16_t val) {
  return ToBigEndian<int16_t>(val);
}
// Returns a byte array holding the value of a int32_t encoded as the
// big-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in big endian, such as for
// network order. Use the native-endian versions when working with values that
// are always in memory, such as when stored in shared-memory (or passed through
// IPC) as a byte buffer. Use the little-endian encoding for storing and reading
// from storage.
constexpr std::array<uint8_t, 4u> I32ToBigEndian(int32_t val) {
  return ToBigEndian<int32_t>(val);
}
// Returns a byte array holding the value of a int64_t encoded as the
// big-endian encoding of the integer.
//
// This is suitable for encoding integers explicitly in big endian, such as for
// network order. Use the native-endian versions when working with values that
// are always in memory, such as when stored in shared-memory (or passed through
// IPC) as a byte buffer. Use the little-endian encoding for storing and reading
// from storage.
constexpr std::array<uint8_t, 8u> I64ToBigEndian(int64_t val) {
  return ToBigEndian<int64_t>(val);
}
// Returns a byte array holding the value of a float encoded as the big-endian
// encoding of the number.
//
// This is suitable for encoding numbers explicitly in big endian, such as for
// network order. Use the native-endian versions when working with values that
// are always in memory, such as when stored in shared-memory (or passed through
// IPC) as a byte buffer. Use the little-endian encoding for storing and reading
// from storage.
constexpr std::array<uint8_t, 4u> FloatToBigEndian(float val) {
  return ToBigEndian<float>(val);
}
// Returns a byte array holding the value of a double encoded as the big-endian
// encoding of the number.
//
// This is suitable for encoding numbers explicitly in big endian, such as for
// network order. Use the native-endian versions when working with values that
// are always in memory, such as when stored in shared-memory (or passed through
// IPC) as a byte buffer. Use the little-endian encoding for storing and reading
// from storage.
constexpr std::array<uint8_t, 8u> DoubleToBigEndian(double val) {
  return ToBigEndian<double>(val);
}

// Returns an Enum with the value in `bytes` interpreted as the native endian
// encoding of its underlying integer type for the machine.
template <typename Enum>
  requires(std::is_enum_v<Enum>)
constexpr Enum EnumFromNativeEndian(
    std::span<const uint8_t, sizeof(Enum)> bytes) {
  return FromNativeEndian<Enum>(bytes);
}

// Returns an Enum with the value in `bytes` interpreted as a little-endian
// encoding of its underlying integer type.
template <typename Enum>
  requires(std::is_enum_v<Enum>)
constexpr Enum EnumFromLittleEndian(
    std::span<const uint8_t, sizeof(Enum)> bytes) {
  return FromLittleEndian<Enum>(bytes);
}

// Returns an Enum with the value in `bytes` interpreted as a big-endian
// encoding of its underlying integer type.
template <typename Enum>
  requires(std::is_enum_v<Enum>)
constexpr Enum EnumFromBigEndian(std::span<const uint8_t, sizeof(Enum)> bytes) {
  return FromBigEndian<Enum>(bytes);
}

// Returns a byte array holding the value of an Enum encoded as the native
// endian encoding of its underlying integer type for the machine.
template <typename Enum>
  requires(std::is_enum_v<Enum>)
constexpr std::array<uint8_t, sizeof(Enum)> EnumToNativeEndian(Enum val) {
  return ToNativeEndian<Enum>(val);
}

// Returns a byte array holding the value of an Enum encoded as the
// little-endian encoding of its underlying integer type.
template <typename Enum>
  requires(std::is_enum_v<Enum>)
constexpr std::array<uint8_t, sizeof(Enum)> EnumToLittleEndian(Enum val) {
  return ToLittleEndian<Enum>(val);
}

// Returns a byte array holding the value of an Enum encoded as the big-endian
// encoding of its underlying integer type.
template <typename Enum>
  requires(std::is_enum_v<Enum>)
constexpr std::array<uint8_t, sizeof(Enum)> EnumToBigEndian(Enum val) {
  return ToBigEndian<Enum>(val);
}

}  // namespace base

#endif  // BASE_NUMERICS_BYTE_CONVERSIONS_H_
