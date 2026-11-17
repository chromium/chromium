// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_ACTUATOR_PROFILE_REGISTRY_PROFILE_REGISTRY_KEYCHAIN_STORE_MAC_H_
#define CHROME_BROWSER_BROWSER_ACTUATOR_PROFILE_REGISTRY_PROFILE_REGISTRY_KEYCHAIN_STORE_MAC_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "base/containers/span.h"
#include "chrome/common/chrome_version.h"

namespace browser_actuator {

// `kSecAttrService` of every profile registry item. `kSecAttrAccount` is the
// profile's sync client ID (`cache_guid`). Items live in
// `kLocalActuationKeychainAccessGroup` (local_actuation_names.h).
inline constexpr char kProfileRegistryKeychainService[] =
    MAC_BUNDLE_IDENTIFIER_STRING ".local-actuation.profile-registry";

// One registry item as read back from the Keychain.
struct ProfileRegistryKeychainItem {
  ProfileRegistryKeychainItem();
  ProfileRegistryKeychainItem(std::string client_id,
                              std::vector<uint8_t> entry_bytes);
  ProfileRegistryKeychainItem(const ProfileRegistryKeychainItem&);
  ProfileRegistryKeychainItem& operator=(const ProfileRegistryKeychainItem&);
  ProfileRegistryKeychainItem(ProfileRegistryKeychainItem&&) noexcept;
  ProfileRegistryKeychainItem& operator=(
      ProfileRegistryKeychainItem&&) noexcept;
  ~ProfileRegistryKeychainItem();

  friend bool operator==(const ProfileRegistryKeychainItem&,
                         const ProfileRegistryKeychainItem&) = default;

  // The item's `kSecAttrAccount`: the profile's sync client ID.
  std::string client_id;
  // The item's `kSecValueData`, unvalidated. Chrome writes a serialized
  // `google.protobuf.Any` wrapping a `ChromeProfileRegistryEntry`.
  std::vector<uint8_t> entry_bytes;
};

// Stores `entry_bytes` (a serialized `google.protobuf.Any`) as the registry
// entry for `client_id`, replacing any existing one. `client_id` must not be
// empty. The item stays on this device and is readable after first unlock.
// Returns false if the Keychain rejects the write.
//
// Blocks on Keychain I/O: call on a sequence that allows blocking.
bool WriteProfileRegistryEntryToKeychain(std::string_view client_id,
                                         base::span<const uint8_t> entry_bytes);

// Removes the registry entry for `client_id`, which must not be empty. Returns
// true if the item was removed or didn't exist.
//
// Blocks on Keychain I/O: call on a sequence that allows blocking.
bool DeleteProfileRegistryEntryFromKeychain(std::string_view client_id);

// Returns every registry item in no particular order, or an empty vector if
// there are none or the query fails. The result includes items written by
// other Chrome channels and user data directories: before deleting, filter on
// the parsed entry's `bundle_id` and `mach_service_name`.
//
// Blocks on Keychain I/O: call on a sequence that allows blocking.
std::vector<ProfileRegistryKeychainItem>
ReadAllProfileRegistryEntriesFromKeychain();

}  // namespace browser_actuator

#endif  // CHROME_BROWSER_BROWSER_ACTUATOR_PROFILE_REGISTRY_PROFILE_REGISTRY_KEYCHAIN_STORE_MAC_H_
