// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise/cloud_storage/policy_utils.h"

#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/odfs_config_private.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace {
constexpr char MicrosoftOneDriveMountAllowed[] = "allowed";
constexpr char MicrosoftOneDriveNoAccountRestriction[] = "common";

}  // namespace

namespace chromeos::cloud_storage {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(prefs::kMicrosoftOneDriveMount,
                               MicrosoftOneDriveMountAllowed);

  base::Value::List account_restrictions_default;
  account_restrictions_default.Append(MicrosoftOneDriveNoAccountRestriction);
  registry->RegisterListPref(prefs::kMicrosoftOneDriveAccountRestrictions,
                             std::move(account_restrictions_default));
#if BUILDFLAG(IS_CHROMEOS_ASH)
  registry->RegisterBooleanPref(prefs::kAllowUserToRemoveODFS, true);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

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
