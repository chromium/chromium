// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_policies.h"

#include "ash/constants/ash_pref_names.h"

namespace chromeos {

KioskPolicies::KioskPolicies(PrefService* pref_service)
    : pref_service_(pref_service) {
  CHECK(pref_service, base::NotFatalUntil::M161);
}

bool KioskPolicies::IsWindowCreationAllowed() const {
  return pref_service_->GetBoolean(ash::prefs::kNewWindowsInKioskAllowed);
}

}  // namespace chromeos
