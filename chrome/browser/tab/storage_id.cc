// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab/storage_id.h"

#include <array>
#include <cstdint>
#include <optional>

#include "base/check.h"
#include "base/check_op.h"
#include "base/containers/span.h"
#include "base/containers/span_reader.h"
#include "base/containers/span_writer.h"
#include "base/numerics/byte_conversions.h"
#include "base/unguessable_token.h"
#include "chrome/browser/tab/protocol/token.pb.h"

namespace tabs {

std::array<uint8_t, kStorageIdBlobSizeBytes> StorageIdToBlob(StorageId id) {
  std::array<uint8_t, kStorageIdBlobSizeBytes> blob;
  base::SpanWriter writer(base::as_writable_byte_span(blob));
  writer.WriteU64LittleEndian(id.GetHighForSerialization());
  writer.WriteU64LittleEndian(id.GetLowForSerialization());
  return blob;
}

StorageId StorageIdFromBlob(base::span<const uint8_t> id) {
  DCHECK_EQ(id.size(), kStorageIdBlobSizeBytes);
  uint64_t high = base::U64FromLittleEndian(id.take_first<8u>());
  uint64_t low = base::U64FromLittleEndian(id.take_first<8u>());
  std::optional<base::UnguessableToken> maybe_token =
      base::UnguessableToken::Deserialize(high, low);
  DCHECK(maybe_token.has_value());
  return *maybe_token;
}

void StorageIdToTokenProto(StorageId id, tabs_pb::Token* token) {
  token->set_high(id.GetHighForSerialization());
  token->set_low(id.GetLowForSerialization());
}

StorageId StorageIdFromTokenProto(const tabs_pb::Token& token) {
  std::optional<base::UnguessableToken> maybe_token =
      base::UnguessableToken::Deserialize(token.high(), token.low());
  DCHECK(maybe_token.has_value());
  return *maybe_token;
}

}  // namespace tabs
