// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_disclaimer.h"

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

constexpr const char* AckPrefForEntryPoint(ScannerEntryPoint entry_point) {
  switch (entry_point) {
    case ScannerEntryPoint::kSmartActionsButton:
      return prefs::kScannerEntryPointDisclaimerAckSmartActionsButton;
    case ScannerEntryPoint::kSunfishSession:
      return prefs::kScannerEntryPointDisclaimerAckSunfishSession;
  }
}

}  // namespace

ScannerDisclaimerType GetScannerDisclaimerType(const PrefService& prefs,
                                               ScannerEntryPoint entry_point) {
  if (!prefs.GetBoolean(prefs::kScannerConsentDisclaimerAccepted)) {
    return ScannerDisclaimerType::kFull;
  }

  if (!prefs.GetBoolean(AckPrefForEntryPoint(entry_point))) {
    return ScannerDisclaimerType::kReminder;
  }

  return ScannerDisclaimerType::kNone;
}

void SetScannerDisclaimerAcked(PrefService& prefs,
                               ScannerEntryPoint entry_point) {
  prefs.SetBoolean(prefs::kScannerConsentDisclaimerAccepted, true);
  prefs.SetBoolean(AckPrefForEntryPoint(entry_point), true);
}

void SetAllScannerDisclaimersUnackedForTest(PrefService& prefs) {
  prefs.SetBoolean(prefs::kScannerConsentDisclaimerAccepted, false);
  prefs.SetBoolean(AckPrefForEntryPoint(ScannerEntryPoint::kSmartActionsButton),
                   false);
  prefs.SetBoolean(AckPrefForEntryPoint(ScannerEntryPoint::kSunfishSession),
                   false);
}

}  // namespace ash
