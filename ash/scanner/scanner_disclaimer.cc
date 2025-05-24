// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_disclaimer.h"

#include <string>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
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

// Returns the type of disclaimer that should override
// `GetScannerDisclaimerType` for debug purposes, or `kNone` if none exists.
ScannerDisclaimerType DebugOverrideDisclaimerType() {
  std::string debug_switch =
      base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
          switches::kScannerDisclaimerDebugOverride);

  if (debug_switch == switches::kScannerDisclaimerDebugOverrideFull) {
    return ScannerDisclaimerType::kFull;
  }

  if (debug_switch == switches::kScannerDisclaimerDebugOverrideReminder) {
    return ScannerDisclaimerType::kReminder;
  }

  return ScannerDisclaimerType::kNone;
}

}  // namespace

ScannerDisclaimerType GetScannerDisclaimerType(const PrefService& prefs,
                                               ScannerEntryPoint entry_point) {
  // Always show the full disclaimer - even if there is a debug override - if
  // the user has not consented yet.
  if (!prefs.GetBoolean(prefs::kScannerConsentDisclaimerAccepted)) {
    return ScannerDisclaimerType::kFull;
  }

  if (ScannerDisclaimerType debug_override = DebugOverrideDisclaimerType();
      debug_override != ScannerDisclaimerType::kNone) {
    return debug_override;
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
