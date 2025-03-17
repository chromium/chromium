// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_disclaimer.h"

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

ScannerDisclaimerType GetScannerDisclaimerType(const PrefService& prefs,
                                               ScannerEntryPoint entry_point) {
  if (!prefs.GetBoolean(prefs::kSunfishConsentDisclaimerAccepted)) {
    return ScannerDisclaimerType::kFull;
  }

  // TODO: crbug.com/383437797 - Return `kReminder` here if the user has not
  // yet acknowledged this entry-point.

  return ScannerDisclaimerType::kNone;
}

void SetScannerDisclaimerAcked(PrefService& prefs,
                               ScannerEntryPoint entry_point) {
  prefs.SetBoolean(prefs::kSunfishConsentDisclaimerAccepted, true);
  // TODO: crbug.com/383437797 - Record that the user has acknowledged the
  // disclaimer for this entry-point.
}

void SetAllScannerDisclaimersUnackedForTest(PrefService& prefs) {
  prefs.SetBoolean(prefs::kSunfishConsentDisclaimerAccepted, false);
  // TODO: crbug.com/383437797 - Unset the prefs for all entry-points.
}

}  // namespace ash
