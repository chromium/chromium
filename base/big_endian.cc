// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/big_endian.h"

#include <string.h>

#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"
#include "base/strings/string_piece.h"

namespace base {

BigEndianReader BigEndianReader::FromStringPiece(
    base::StringPiece string_piece) {
  return BigEndianReader(base::as_byte_span(string_piece));
}

BigEndianReader::BigEndianReader(const uint8_t* buf, size_t len)
    : ptr_(buf), end_(ptr_ + len) {
  // Ensure `len` does not cause `end_` to wrap around.
  CHECK_GE(end_, ptr_);
}

BigEndianReader::BigEndianReader(base::span<const uint8_t> buf)
    : ptr_(buf.data()), end_(buf.data() + buf.size()) {}

bool BigEndianReader::Skip(size_t len) {
  if (len > remaining()) {
    return false;
  }
  ptr_ += len;
  return true;
}

bool BigEndianReader::ReadBytes(void* out, size_t len) {
  if (len > remaining()) {
    return false;
  }
  memcpy(out, ptr_, len);
  ptr_ += len;
  return true;
}

bool BigEndianReader::ReadPiece(base::StringPiece* out, size_t len) {
  if (len > remaining()) {
    return false;
  }
  *out = base::StringPiece(reinterpret_cast<const char*>(ptr_), len);
  ptr_ += len;
  return true;
}

bool BigEndianReader::ReadSpan(base::span<const uint8_t>* out, size_t len) {
  if (len > remaining()) {
    return false;
  }
  *out = base::make_span(ptr_, len);
  ptr_ += len;
  return true;
}

bool BigEndianReader::ReadU8(uint8_t* value) {
  std::optional<span<const uint8_t, 1u>> bytes = ReadFixedSpan<1u>();
  if (!bytes.has_value()) {
    return false;
  }
  *value = numerics::U8FromBigEndian(*bytes);
  return true;
}

bool BigEndianReader::ReadU16(uint16_t* value) {
  std::optional<span<const uint8_t, 2u>> bytes = ReadFixedSpan<2u>();
  if (!bytes.has_value()) {
    return false;
  }
  *value = numerics::U16FromBigEndian(*bytes);
  return true;
}

bool BigEndianReader::ReadU32(uint32_t* value) {
  std::optional<span<const uint8_t, 4u>> bytes = ReadFixedSpan<4u>();
  if (!bytes.has_value()) {
    return false;
  }
  *value = numerics::U32FromBigEndian(*bytes);
  return true;
}

bool BigEndianReader::ReadU64(uint64_t* value) {
  std::optional<span<const uint8_t, 8u>> bytes = ReadFixedSpan<8u>();
  if (!bytes.has_value()) {
    return false;
  }
  *value = numerics::U64FromBigEndian(*bytes);
  return true;
}

bool BigEndianReader::ReadU8LengthPrefixed(std::string_view* out) {
  uint8_t len;
  if (!ReadU8(&len)) {
    return false;
  }
  const bool ok = ReadPiece(out, len);
  if (!ok) {
    ptr_ -= 1u;  // Undo the ReadU8.
  }
  return ok;
}

bool BigEndianReader::ReadU16LengthPrefixed(std::string_view* out) {
  uint16_t len;
  if (!ReadU16(&len)) {
    return false;
  }
  const bool ok = ReadPiece(out, len);
  if (!ok) {
    ptr_ -= 2u;  // Undo the ReadU16.
  }
  return ok;
}

BigEndianWriter::BigEndianWriter(char* buf, size_t len)
    : ptr_(buf), end_(ptr_ + len) {
  // Ensure `len` does not cause `end_` to wrap around.
  CHECK_GE(end_, ptr_);
}

bool BigEndianWriter::Skip(size_t len) {
  if (len > remaining()) {
    return false;
  }
  ptr_ += len;
  return true;
}

bool BigEndianWriter::WriteBytes(const void* buf, size_t len) {
  if (len > remaining()) {
    return false;
  }
  memcpy(ptr_, buf, len);
  ptr_ += len;
  return true;
}

template <typename T>
bool BigEndianWriter::Write(T value) {
  if (sizeof(T) > remaining()) {
    return false;
  }
  WriteBigEndian<T>(ptr_, value);
  ptr_ += sizeof(T);
  return true;
}

bool BigEndianWriter::WriteU8(uint8_t value) {
  return Write(value);
}

bool BigEndianWriter::WriteU16(uint16_t value) {
  return Write(value);
}

bool BigEndianWriter::WriteU32(uint32_t value) {
  return Write(value);
}

bool BigEndianWriter::WriteU64(uint64_t value) {
  return Write(value);
}

}  // namespace base
