// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_PROVIDER_CONFIG_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_PROVIDER_CONFIG_H_

#include <string>
#include <vector>

#include "base/containers/flat_set.h"
#include "components/unexportable_keys/unexportable_key_id.h"
#include "crypto/unexportable_key.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace unexportable_keys {

class UnexportableKeyService;

// An enum to define the intended use of the unexportable key.
enum class KeyPurpose {
  kRefreshTokenBinding,
  kDeviceBoundSessionCredentials,
  // Temporary, will be removed when replaced by DBSC Standard.
  kDeviceBoundSessionCredentialsPrototype,
};

// Returns the default config for the `UnexportableKeyProvider`. This config
// should only be used to test for platform support of unexportable keys or to
// create keys that are cleaned up shortly after and don't survive browser
// restarts.
crypto::UnexportableKeyProvider::Config GetDefaultConfig();

// Returns a config for the `UnexportableKeyProvider` for the given
// `user_data_dir`. This config's tag is used on macOS to group related keys in
// the Keychain so they can be queried and deleted together.
//
// The tag is constructed to ensure keys are uniquely scoped to a specific
// user data directory, which is critical for cleaning up orphaned keys when a
// user profile is deleted or an incognito session ends. It is composed of:
// - The bundle and team identifiers to scope it to the application.
// - A hash of the current profile's user data directory.
crypto::UnexportableKeyProvider::Config GetConfigForUserDataDir(
    const base::FilePath& user_data_dir);

// Returns a config for the `UnexportableKeyProvider` for the given
// `profile_path`. This config's tag is used on macOS to group related keys in
// the Keychain so they can be queried and deleted together.
//
// The tag is constructed to ensure keys are uniquely scoped to a specific
// profile path, which is critical for cleaning up orphaned keys when a
// user profile is deleted or an incognito session ends. It is composed of:
// - The bundle and team identifiers to scope it to the application.
// - A hash of the current profile's user data directory.
// - The profile's name to uniquely identify the profile.
crypto::UnexportableKeyProvider::Config GetConfigForProfilePath(
    const base::FilePath& profile_path);

// Returns a config for the `UnexportableKeyProvider` for the given
// `profile`. This config's tag is used on macOS to group related keys in
// the Keychain so they can be queried and deleted together.
//
// The tag is constructed to ensure keys are uniquely scoped to a specific
// profile, which is critical for cleaning up orphaned keys when a user profile
// is deleted or an incognito session ends. It is composed of:
// - The bundle and team identifiers to scope it to the application.
// - A hash of the current profile's user data directory.
// - The profile's name to uniquely identify the profile.
// - A hash of the profile's creation time to distinguish OTR profiles that have
//   dedicated cleanup logic.
crypto::UnexportableKeyProvider::Config GetConfigForProfile(
    const Profile& profile);

// Returns a config for the `UnexportableKeyProvider` for the given `profile`
// and `relative_partition_path`. This config's tag is used on macOS to group
// related keys in the Keychain so they can be queried and deleted together.
//
// `relative_partition_path` should be relative to `profile.GetPath()`.
//
// The tag is constructed to ensure keys are uniquely scoped to a specific
// storage partition path, which is critical for cleaning up orphaned keys when
// they are no longer used. It is composed of:
// - The bundle and team identifiers to scope it to the application.
// - A hash of the current profile's user data directory.
// - The profile's name to uniquely identify the profile.
// - A hash of the profile's creation time to distinguish OTR profiles that have
//   dedicated cleanup logic.
// - A hash of the storage partition path to uniquely identify the storage
//   partition.
// - A string representing the key's `purpose` (e.g., "dbsc-standard", "lst").
//
// This allows for safe, bulk deletion of keys that are no longer in use without
// affecting keys from other storage partitions or for other purposes.
crypto::UnexportableKeyProvider::Config
GetConfigForStoragePartitionPathAndPurpose(
    const Profile& profile,
    const base::FilePath& relative_partition_path,
    KeyPurpose purpose);

// Returns the application tag for the given `config`.
// Returns an empty string if the platform does not support application tags or
// if the config does not have one.
std::string GetApplicationTag(crypto::UnexportableKeyProvider::Config config);

// Filters `key_ids`, removing ids where the key's tag cannot be obtained, or
// where the tag is prefixed by one of the `active_application_tag_prefixes`.
// Returns the number of keys removed.
size_t FilterUnexportableKeysByActiveApplicationTags(
    std::vector<UnexportableKeyId>& key_ids,
    UnexportableKeyService& key_service,
    const base::flat_set<std::string>& active_application_tag_prefixes);

}  // namespace unexportable_keys

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_PROVIDER_CONFIG_H_
