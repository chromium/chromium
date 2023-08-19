// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/kiosk_policies.h"

#include "chrome/common/pref_names.h"

namespace chromeos {

KioskPolicies::KioskPolicies(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service);
}

bool KioskPolicies::IsWindowCreationAllowed() const {
  return pref_service_->GetBoolean(prefs::kNewWindowsInKioskAllowed);
}

}  // namespace chromeos
