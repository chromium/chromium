// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/nearby_sharing/certificates/nearby_share_encrypted_metadata_key.h"

#include <stdint.h>

#include <array>
#include <utility>

#include "base/check.h"
#include "base/containers/span.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"

NearbyShareEncryptedMetadataKey::NearbyShareEncryptedMetadataKey(
    base::span<const uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt>
        salt,
    base::span<const uint8_t, kNearbyShareNumBytesMetadataEncryptionKey>
        encrypted_key) {
  base::span(salt_).copy_from(salt);
  base::span(encrypted_key_).copy_from(encrypted_key);
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
