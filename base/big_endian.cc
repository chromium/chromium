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
    :  // TODO(crbug.com/40284755): Remove this constructor entirely, callers
       // should use span constructor.
      UNSAFE_BUFFERS(buffer_(buf, len)) {}

BigEndianReader::BigEndianReader(base::span<const uint8_t> buffer)
    : buffer_(buffer) {}

BigEndianReader::~BigEndianReader() = default;

bool BigEndianReader::Skip(size_t len) {
  if (len > remaining()) {
    return false;
  }
  buffer_ = buffer_.subspan(len);
  return true;
}

bool BigEndianReader::ReadU8(uint8_t* value) {
  std::array<uint8_t, 1u> bytes;
  if (!ReadBytes<1u>(bytes)) {
    return false;
  }
  *value = numerics::U8FromBigEndian(bytes);
  return true;
}

bool BigEndianReader::ReadU16(uint16_t* value) {
  std::array<uint8_t, 2u> bytes;
  if (!ReadBytes<2u>(bytes)) {
    return false;
  }
  *value = numerics::U16FromBigEndian(bytes);
  return true;
}

bool BigEndianReader::ReadU32(uint32_t* value) {
  std::array<uint8_t, 4u> bytes;
  if (!ReadBytes<4u>(bytes)) {
    return false;
  }
  *value = numerics::U32FromBigEndian(bytes);
  return true;
}

bool BigEndianReader::ReadU64(uint64_t* value) {
  std::array<uint8_t, 8u> bytes;
  if (!ReadBytes<8u>(bytes)) {
    return false;
  }
  *value = numerics::U64FromBigEndian(bytes);
  return true;
}

bool BigEndianReader::ReadPiece(base::StringPiece* out, size_t len) {
  if (len > remaining()) {
    return false;
  }
  auto [view, remain] = buffer_.split_at(len);
  *out = base::StringPiece(reinterpret_cast<const char*>(view.data()),
                           view.size());
  buffer_ = remain;
  return true;
}

std::optional<span<const uint8_t>> BigEndianReader::ReadSpan(
    base::StrictNumeric<size_t> n) {
  if (remaining() < size_t{n}) {
    return std::nullopt;
  }
  auto [consume, remain] = buffer_.split_at(n);
  buffer_ = remain;
  return {consume};
}

bool BigEndianReader::ReadBytes(span<uint8_t> out) {
  if (remaining() < out.size()) {
    return false;
  }
  auto [consume, remain] = buffer_.split_at(out.size());
  buffer_ = remain;
  out.copy_from(consume);
  return true;
}

bool BigEndianReader::ReadU8LengthPrefixed(std::string_view* out) {
  span<const uint8_t> rollback = buffer_;
  uint8_t len;
  if (!ReadU8(&len)) {
    return false;
  }
  const bool ok = ReadPiece(out, len);
  if (!ok) {
    buffer_ = rollback;  // Undo the ReadU8.
  }
  return ok;
}

bool BigEndianReader::ReadU16LengthPrefixed(std::string_view* out) {
  span<const uint8_t> rollback = buffer_;
  uint16_t len;
  if (!ReadU16(&len)) {
    return false;
  }
  const bool ok = ReadPiece(out, len);
  if (!ok) {
    buffer_ = rollback;  // Undo the ReadU8.
  }
  return ok;
}

BigEndianWriter::BigEndianWriter(span<uint8_t> buffer) : buffer_(buffer) {}

BigEndianWriter::BigEndianWriter(char* buf, size_t len)
    :  // TODO(crbug.com/40284755): Remove this constructor entirely, callers
       // should use span constructor.
      UNSAFE_BUFFERS(buffer_(reinterpret_cast<uint8_t*>(buf), len)) {}

BigEndianWriter::~BigEndianWriter() = default;

bool BigEndianWriter::Skip(size_t len) {
  if (len > remaining()) {
    return false;
  }
  buffer_ = buffer_.subspan(len);
  return true;
}

bool BigEndianWriter::WriteBytes(const void* buf, size_t len) {
  return WriteSpan(
      // TODO(crbug.com/40284755): Remove WriteBytes() entirely, callers
      // should use WriteSpan()..
      UNSAFE_BUFFERS((base::span(static_cast<const uint8_t*>(buf), len))));
}

bool BigEndianWriter::WriteSpan(base::span<const uint8_t> bytes) {
  if (remaining() < bytes.size()) {
    return false;
  }
  auto [write, remain] = buffer_.split_at(bytes.size());
  write.copy_from(bytes);
  buffer_ = remain;
  return true;
}

bool BigEndianWriter::WriteU8(uint8_t value) {
  // TODO(danakj) this span constructor should be implicit.
  return WriteFixedSpan(base::span(numerics::U8ToBigEndian(value)));
}

bool BigEndianWriter::WriteU16(uint16_t value) {
  // TODO(danakj) this span constructor should be implicit.
  return WriteFixedSpan(base::span(numerics::U16ToBigEndian(value)));
}

bool BigEndianWriter::WriteU32(uint32_t value) {
  // TODO(danakj) this span constructor should be implicit.
  return WriteFixedSpan(base::span(numerics::U32ToBigEndian(value)));
}

bool BigEndianWriter::WriteU64(uint64_t value) {
  // TODO(danakj) this span constructor should be implicit.
  return WriteFixedSpan(base::span(numerics::U64ToBigEndian(value)));
}

}  // namespace base
