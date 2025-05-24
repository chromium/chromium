// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_ENCRYPTED_METADATA_KEY_H_
#define CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_ENCRYPTED_METADATA_KEY_H_

#include <stdint.h>

#include <array>

#include "base/containers/span.h"
#include "chrome/browser/nearby_sharing/certificates/constants.h"

// Holds the encrypted symmetric key--the key used to encrypt user/device
// metatdata--as well as the salt used to encrypt the key.
struct NearbyShareEncryptedMetadataKey {
 public:
  NearbyShareEncryptedMetadataKey(
      base::span<const uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt>
          salt,
      base::span<const uint8_t, kNearbyShareNumBytesMetadataEncryptionKey>
          encrypted_key);
  NearbyShareEncryptedMetadataKey(const NearbyShareEncryptedMetadataKey&);
  NearbyShareEncryptedMetadataKey& operator=(
      const NearbyShareEncryptedMetadataKey&);
  NearbyShareEncryptedMetadataKey(NearbyShareEncryptedMetadataKey&&);
  NearbyShareEncryptedMetadataKey& operator=(NearbyShareEncryptedMetadataKey&&);
  ~NearbyShareEncryptedMetadataKey();

  base::span<const uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt>
  salt() const {
    return salt_;
  }
  base::span<const uint8_t, kNearbyShareNumBytesMetadataEncryptionKey>
  encrypted_key() const {
    return encrypted_key_;
  }

 private:
  std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKeySalt> salt_;
  std::array<uint8_t, kNearbyShareNumBytesMetadataEncryptionKey> encrypted_key_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_CERTIFICATES_NEARBY_SHARE_ENCRYPTED_METADATA_KEY_H_
