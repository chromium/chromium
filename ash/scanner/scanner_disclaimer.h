// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SCANNER_SCANNER_DISCLAIMER_H_
#define ASH_SCANNER_SCANNER_DISCLAIMER_H_

#include "ash/ash_export.h"

class PrefService;

namespace ash {

// The type of Scanner disclaimer that should be shown.
enum class ASH_EXPORT ScannerDisclaimerType {
  // No disclaimer should be shown.
  kNone,
  // The reminder disclaimer should be shown.
  kReminder,
  // The full non-reminder disclaimer should be shown.
  kFull,
};

// Returns the type of Scanner disclaimer that should be shown according to the
// given `PrefService`.
// Does NOT check whether Scanner-related UI can be shown
// (`ScannerController::CanShowUiForShell()`).
ASH_EXPORT ScannerDisclaimerType
GetScannerDisclaimerType(const PrefService& prefs);

// Sets the Scanner disclaimer as acknowledged by the user in the given
// `PrefService`.
ASH_EXPORT void SetScannerDisclaimerAcked(PrefService& prefs);

// Sets the Scanner disclaimer as unacknowledged by the user in the given
// `PrefService` for testing purposes.
ASH_EXPORT void SetScannerDisclaimerUnackedForTest(PrefService& prefs);

}  // namespace ash

#endif  // ASH_SCANNER_SCANNER_DISCLAIMER_H_
