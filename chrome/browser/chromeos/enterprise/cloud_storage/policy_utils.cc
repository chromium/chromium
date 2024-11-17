// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise/cloud_storage/policy_utils.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/odfs_config_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace chromeos::cloud_storage {

extensions::api::odfs_config_private::Mount GetMicrosoftOneDriveMount(
    const Profile* profile) {
  const PrefService* pref_service = profile->GetPrefs();
  const std::string& mount_state =
      pref_service->GetString(prefs::kMicrosoftOneDriveMount);
  return extensions::api::odfs_config_private::ParseMount(mount_state);
}

base::Value::List GetMicrosoftOneDriveAccountRestrictions(
    const Profile* profile) {
  const PrefService* pref_service = profile->GetPrefs();
  return pref_service->GetList(prefs::kMicrosoftOneDriveAccountRestrictions)
      .Clone();
}

}  // namespace chromeos::cloud_storage
