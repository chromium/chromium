// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/enterprise/cloud_storage/policy_utils.h"

#include "base/values.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"

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
}

}  // namespace chromeos::cloud_storage
