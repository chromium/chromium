// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/big_endian.h"

#include <string.h>

#include <string_view>

#include "base/numerics/byte_conversions.h"
#include "base/numerics/checked_math.h"

namespace base {

BigEndianReader BigEndianReader::FromStringPiece(std::string_view view) {
  return BigEndianReader(as_byte_span(view));
}

BigEndianReader::BigEndianReader(const uint8_t* buf, size_t len)
    :  // TODO(crbug.com/40284755): Remove this constructor entirely, callers
       // should use span constructor.
      UNSAFE_BUFFERS(buffer_(buf, len)) {}

BigEndianReader::BigEndianReader(span<const uint8_t> buffer)
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
  *value = U8FromBigEndian(bytes);
  return true;
}

bool BigEndianReader::ReadI8(int8_t* value) {
  std::array<uint8_t, 1u> bytes;
  if (!ReadBytes<1u>(bytes)) {
    return false;
  }
  *value = static_cast<int8_t>(numerics::U8FromBigEndian(bytes));
  return true;
}

bool BigEndianReader::ReadU16(uint16_t* value) {
  std::array<uint8_t, 2u> bytes;
  if (!ReadBytes<2u>(bytes)) {
    return false;
  }
  *value = U16FromBigEndian(bytes);
  return true;
}

bool BigEndianReader::ReadI16(int16_t* value) {
  std::array<uint8_t, 2u> bytes;
  if (!ReadBytes<2u>(bytes)) {
    return false;
  }
  *value = static_cast<int16_t>(numerics::U16FromBigEndian(bytes));
  return true;
}

bool BigEndianReader::ReadU32(uint32_t* value) {
  std::array<uint8_t, 4u> bytes;
  if (!ReadBytes<4u>(bytes)) {
    return false;
  }
  *value = U32FromBigEndian(bytes);
  return true;
}

bool BigEndianReader::ReadI32(int32_t* value) {
  std::array<uint8_t, 4u> bytes;
  if (!ReadBytes<4u>(bytes)) {
    return false;
  }
  *value = static_cast<int32_t>(numerics::U32FromBigEndian(bytes));
  return true;
}

bool BigEndianReader::ReadU64(uint64_t* value) {
  std::array<uint8_t, 8u> bytes;
  if (!ReadBytes<8u>(bytes)) {
    return false;
  }
  *value = U64FromBigEndian(bytes);
  return true;
}

bool BigEndianReader::ReadI64(int64_t* value) {
  std::array<uint8_t, 8u> bytes;
  if (!ReadBytes<8u>(bytes)) {
    return false;
  }
  *value = static_cast<int64_t>(numerics::U64FromBigEndian(bytes));
  return true;
}

bool BigEndianReader::ReadPiece(std::string_view* out, size_t len) {
  if (len > remaining()) {
    return false;
  }
  auto [view, remain] = buffer_.split_at(len);
  *out =
      std::string_view(reinterpret_cast<const char*>(view.data()), view.size());
  buffer_ = remain;
  return true;
}

std::optional<span<const uint8_t>> BigEndianReader::ReadSpan(
    StrictNumeric<size_t> n) {
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

}  // namespace base
