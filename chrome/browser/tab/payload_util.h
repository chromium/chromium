// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_PAYLOAD_UTIL_H_
#define CHROME_BROWSER_TAB_PAYLOAD_UTIL_H_

#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "chrome/browser/tab/storage_id.h"

namespace tabs {

// Off the record payloads are expected to be encrypted.

// On desktop, off the record tabs are not stored.

// On Android, the key used for encryption is stored in the app's bundle and may
// be returned by the OS if the app is resumed. Explicitly closing the app, or
// an update to the app will invalidate the key and cause the stored data to be
// lost see CipherFactory.java for details.

// Generates a new key for sealing OTR payloads.
std::vector<uint8_t> GenerateKeyForOtrPayloads();

// Seals a payload with `key` using a random nonce and `storage_id` as
// additional metadata. The nonce is appended to the encrypted payload. A random
// nonce should be safe as the data for OTR tabs is frequently wiped out and the
// key is rotated when not restored from the bundle.
std::vector<uint8_t> SealPayload(base::span<const uint8_t> key,
                                 base::span<const uint8_t> payload,
                                 StorageId storage_id);

// Opens a payload with `key` and the additional metadata `storage_id`. The
// nonce is expected to be appended to the encrypted payload. Returns
// std::nullopt if the decryption fails or the `encrypted_payload` is too short
// to contain the nonce.
std::optional<std::vector<uint8_t>> OpenPayload(
    base::span<const uint8_t> key,
    base::span<const uint8_t> encrypted_payload,
    StorageId storage_id);

}  // namespace tabs

#endif  // CHROME_BROWSER_TAB_PAYLOAD_UTIL_H_
