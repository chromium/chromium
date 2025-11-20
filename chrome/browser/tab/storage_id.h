// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_STORAGE_ID_H_
#define CHROME_BROWSER_TAB_STORAGE_ID_H_

#include "base/unguessable_token.h"

namespace tabs_pb {
class Token;
}  // namespace tabs_pb

namespace tabs {

// Define a type alias for storage IDs. The underlying type may change before
// launch. Using a type alias allows for easier migration.
using StorageId = base::UnguessableToken;

inline constexpr size_t kStorageIdBlobSizeBytes = sizeof(uint64_t) * 2;

// Converts a StorageId to a blob of bytes.
std::array<uint8_t, kStorageIdBlobSizeBytes> StorageIdToBlob(StorageId id);

// Converts a blob of bytes to a StorageId.
StorageId StorageIdFromBlob(base::span<const uint8_t> id);

// Converts a StorageId to a Token proto.
void StorageIdToTokenProto(StorageId id, tabs_pb::Token* token);

// Converts a Token proto to a StorageId.
StorageId StorageIdFromTokenProto(const tabs_pb::Token& token);

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_STORAGE_ID_H_
