// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ENTERPRISE_CLOUD_STORAGE_POLICY_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_ENTERPRISE_CLOUD_STORAGE_POLICY_UTILS_H_

#include "base/values.h"

class PrefRegistrySimple;
class Profile;

namespace extensions::api::odfs_config_private {
enum class Mount;
}  // namespace extensions::api::odfs_config_private

namespace chromeos::cloud_storage {

// Registers the profile prefs related to cloud storage.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Returns the mount mode derived from the MicrosoftOneDriveMount policy.
extensions::api::odfs_config_private::Mount GetMicrosoftOneDriveMount(
    const Profile* profile);

// Returns the mount mode derived from the MicrosoftOneDriveAccountRestrictions
// policy.
base::Value::List GetMicrosoftOneDriveAccountRestrictions(
    const Profile* profile);

}  // namespace chromeos::cloud_storage

#endif  // CHROME_BROWSER_CHROMEOS_ENTERPRISE_CLOUD_STORAGE_POLICY_UTILS_H_
