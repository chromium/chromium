// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_POLICIES_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_POLICIES_H_

#include "components/prefs/pref_service.h"

namespace chromeos {

// Helper class for policies related to the kiosk session.
class KioskPolicies {
 public:
  explicit KioskPolicies(PrefService* pref_service);
  ~KioskPolicies() = default;

  // Returns `false` if `prefs::kNewWindowsInKioskAllowed` preference is not
  // found in the pref service, otherwise returns its value.
  bool IsWindowCreationAllowed() const;

 private:
  raw_ptr<PrefService, DanglingUntriaged> pref_service_;
};

}  // namespace chromeos
#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_POLICIES_H_
