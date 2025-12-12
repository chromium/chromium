// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_PROVIDER_CONFIG_H_
#define CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_PROVIDER_CONFIG_H_

#include "crypto/unexportable_key.h"

class Profile;

namespace base {
class FilePath;
}  // namespace base

namespace unexportable_keys {

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

// Returns a config for the `UnexportableKeyProvider` for the given
// `profile` and `purpose`. This config's tag is used on macOS to group related
// keys in the Keychain so they can be queried and deleted together.
//
// The tag is constructed to ensure keys are uniquely scoped to a specific
// profile and use case, which is critical for cleaning up orphaned keys when a
// profile is deleted or an incognito session ends. It is composed of:
// - The bundle and team identifiers to scope it to the application.
// - A hash of the current profile's user data directory.
// - The profile's name to uniquely identify the profile.
// - A hash of the profile's creation time to distinguish OTR profiles that have
//   dedicated cleanup logic.
// - A string representing the key's `purpose` (e.g., "dbsc", "lst").
//
// This allows for safe, bulk deletion of keys that are no longer in use without
// affecting keys from other profiles or for other purposes.
crypto::UnexportableKeyProvider::Config GetConfigForProfileAndPurpose(
    const Profile& profile,
    KeyPurpose purpose);

}  // namespace unexportable_keys

#endif  // CHROME_BROWSER_SIGNIN_BOUND_SESSION_CREDENTIALS_UNEXPORTABLE_KEY_PROVIDER_CONFIG_H_
