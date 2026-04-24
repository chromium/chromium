// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise/cloud_storage/pref_utils.h"

#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "base/values.h"
#include "components/prefs/pref_registry_simple.h"

namespace {
constexpr char MicrosoftOneDriveMountAllowed[] = "allowed";
constexpr char MicrosoftOneDriveNoAccountRestriction[] = "common";

}  // namespace

namespace chromeos::cloud_storage {

void RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterStringPref(ash::prefs::kMicrosoftOneDriveMount,
                               MicrosoftOneDriveMountAllowed);

  base::ListValue account_restrictions_default;
  account_restrictions_default.Append(MicrosoftOneDriveNoAccountRestriction);
  registry->RegisterListPref(ash::prefs::kMicrosoftOneDriveAccountRestrictions,
                             std::move(account_restrictions_default));
  registry->RegisterBooleanPref(ash::prefs::kAllowUserToRemoveODFS, true);
  registry->RegisterBooleanPref(ash::prefs::kM365SupportedLinkDefaultSet,
                                false);
}

}  // namespace chromeos::cloud_storage
