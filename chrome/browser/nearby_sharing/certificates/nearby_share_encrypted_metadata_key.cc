// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"

#include <utility>

#include "base/check.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"

NearbyShareEncryptedMetadataKey::NearbyShareEncryptedMetadataKey(
    std::vector<uint8_t> salt,
    std::vector<uint8_t> encrypted_key)
    : salt_(std::move(salt)), encrypted_key_(std::move(encrypted_key)) {
  DCHECK_EQ(kNearbyShareNumBytesMetadataEncryptionKeySalt, salt_.size());
  DCHECK_EQ(kNearbyShareNumBytesMetadataEncryptionKey, encrypted_key_.size());
}

NearbyShareEncryptedMetadataKey::NearbyShareEncryptedMetadataKey(
    const NearbyShareEncryptedMetadataKey&) = default;

NearbyShareEncryptedMetadataKey& NearbyShareEncryptedMetadataKey::operator=(
    const NearbyShareEncryptedMetadataKey&) = default;

NearbyShareEncryptedMetadataKey::NearbyShareEncryptedMetadataKey(
    NearbyShareEncryptedMetadataKey&&) = default;

NearbyShareEncryptedMetadataKey& NearbyShareEncryptedMetadataKey::operator=(
    NearbyShareEncryptedMetadataKey&&) = default;

NearbyShareEncryptedMetadataKey::~NearbyShareEncryptedMetadataKey() = default;
