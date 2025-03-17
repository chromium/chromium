// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_disclaimer.h"

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

bool ShouldShowScannerDisclaimer(const PrefService& prefs) {
  return !prefs.GetBoolean(prefs::kSunfishConsentDisclaimerAccepted);
}

void SetScannerDisclaimerAcked(PrefService& prefs) {
  prefs.SetBoolean(prefs::kSunfishConsentDisclaimerAccepted, true);
}

void SetScannerDisclaimerUnackedForTest(PrefService& prefs) {
  prefs.SetBoolean(prefs::kSunfishConsentDisclaimerAccepted, false);
}

}  // namespace ash
