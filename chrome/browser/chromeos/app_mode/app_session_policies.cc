// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/app_mode/app_session_policies.h"

#include "chrome/common/pref_names.h"

namespace chromeos {

AppSessionPolicies::AppSessionPolicies(PrefService* pref_service)
    : pref_service_(pref_service) {
  DCHECK(pref_service);
}

bool AppSessionPolicies::IsWindowCreationAllowed() const {
  return pref_service_->GetBoolean(prefs::kNewWindowsInKioskAllowed);
}

}  // namespace chromeos
