// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_COLOCATION_COLOCATION_PROOF_KEYCHAIN_STORE_MAC_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_COLOCATION_COLOCATION_PROOF_KEYCHAIN_STORE_MAC_H_

#include <cstdint>
#include <string_view>

#include "base/containers/span.h"

namespace browser_actuator {

// Data Protection Keychain access group that holds co-location proofs. Chrome
// shares this group with the local apps that read the proof, so it is the
// same on every channel.
extern const char kColocationProofKeychainAccessGroup[];

// `kSecAttrService` of every co-location proof item. `kSecAttrAccount` is the
// profile's sync client ID (`cache_guid`).
extern const char kColocationProofKeychainService[];

// Stores `proof` (a serialized `google.protobuf.Any`) as the co-location proof
// for `client_id`, replacing any existing one. The item never syncs to iCloud
// and stays readable after first unlock while Chrome is closed. Returns false
// if the Keychain rejects the write.
//
// Blocks on Keychain I/O: call on a sequence that allows blocking.
bool WriteColocationProofToKeychain(std::string_view client_id,
                                    base::span<const uint8_t> proof);

// Removes the co-location proof for `client_id`. Returns true if the item was
// removed or didn't exist.
//
// Blocks on Keychain I/O: call on a sequence that allows blocking.
bool DeleteColocationProofFromKeychain(std::string_view client_id);

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_COLOCATION_COLOCATION_PROOF_KEYCHAIN_STORE_MAC_H_
