// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/scanner/scanner_disclaimer.h"

#include "ash/constants/ash_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

void RegisterPrefs(PrefRegistrySimple& registry) {
  registry.RegisterBooleanPref(prefs::kSunfishConsentDisclaimerAccepted, false);
}

TEST(ScannerDisclaimerTest, InitialValue) {
  TestingPrefServiceSimple prefs;
  RegisterPrefs(*prefs.registry());

  EXPECT_EQ(GetScannerDisclaimerType(prefs), ScannerDisclaimerType::kFull);
}

TEST(ScannerDisclaimerTest, AfterAcknowledge) {
  TestingPrefServiceSimple prefs;
  RegisterPrefs(*prefs.registry());

  SetScannerDisclaimerAcked(prefs);

  EXPECT_EQ(GetScannerDisclaimerType(prefs), ScannerDisclaimerType::kNone);
}

TEST(ScannerDisclaimerTest, AcknowledgeIsIdempotent) {
  TestingPrefServiceSimple prefs;
  RegisterPrefs(*prefs.registry());

  SetScannerDisclaimerAcked(prefs);
  SetScannerDisclaimerAcked(prefs);

  EXPECT_EQ(GetScannerDisclaimerType(prefs), ScannerDisclaimerType::kNone);
}

}  // namespace
}  // namespace ash
