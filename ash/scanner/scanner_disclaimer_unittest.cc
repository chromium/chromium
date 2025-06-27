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
  registry.RegisterBooleanPref(prefs::kScannerConsentDisclaimerAccepted, false);
  registry.RegisterBooleanPref(
      prefs::kScannerEntryPointDisclaimerAckSmartActionsButton, false);
  registry.RegisterBooleanPref(
      prefs::kScannerEntryPointDisclaimerAckSunfishSession, false);
}

TEST(ScannerDisclaimerTest, InitialValues) {
  TestingPrefServiceSimple prefs;
  RegisterPrefs(*prefs.registry());

  EXPECT_EQ(
      GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSmartActionsButton),
      ScannerDisclaimerType::kFull);
  EXPECT_EQ(GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSunfishSession),
            ScannerDisclaimerType::kFull);
}

TEST(ScannerDisclaimerTest, AfterSmartActionsButtonAcknowledge) {
  TestingPrefServiceSimple prefs;
  RegisterPrefs(*prefs.registry());

  SetScannerDisclaimerAcked(prefs, ScannerEntryPoint::kSmartActionsButton);

  EXPECT_EQ(
      GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSmartActionsButton),
      ScannerDisclaimerType::kNone);
  EXPECT_EQ(GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSunfishSession),
            ScannerDisclaimerType::kReminder);
}

TEST(ScannerDisclaimerTest, AfterSunfishSessionAcknowledge) {
  TestingPrefServiceSimple prefs;
  RegisterPrefs(*prefs.registry());

  SetScannerDisclaimerAcked(prefs, ScannerEntryPoint::kSunfishSession);

  EXPECT_EQ(
      GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSmartActionsButton),
      ScannerDisclaimerType::kReminder);
  EXPECT_EQ(GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSunfishSession),
            ScannerDisclaimerType::kNone);
}

TEST(ScannerDisclaimerTest,
     AfterSmartActionsButtonThenSunfishSessionAcknowledge) {
  TestingPrefServiceSimple prefs;
  RegisterPrefs(*prefs.registry());

  SetScannerDisclaimerAcked(prefs, ScannerEntryPoint::kSmartActionsButton);
  SetScannerDisclaimerAcked(prefs, ScannerEntryPoint::kSunfishSession);

  EXPECT_EQ(
      GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSmartActionsButton),
      ScannerDisclaimerType::kNone);
  EXPECT_EQ(GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSunfishSession),
            ScannerDisclaimerType::kNone);
}

TEST(ScannerDisclaimerTest,
     AfterSunfishSessionThenSmartActionsButtonAcknowledge) {
  TestingPrefServiceSimple prefs;
  RegisterPrefs(*prefs.registry());

  SetScannerDisclaimerAcked(prefs, ScannerEntryPoint::kSunfishSession);
  SetScannerDisclaimerAcked(prefs, ScannerEntryPoint::kSmartActionsButton);

  EXPECT_EQ(
      GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSmartActionsButton),
      ScannerDisclaimerType::kNone);
  EXPECT_EQ(GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSunfishSession),
            ScannerDisclaimerType::kNone);
}

TEST(ScannerDisclaimerTest, AcknowledgeIsIdempotent) {
  TestingPrefServiceSimple prefs;
  RegisterPrefs(*prefs.registry());

  SetScannerDisclaimerAcked(prefs, ScannerEntryPoint::kSmartActionsButton);
  SetScannerDisclaimerAcked(prefs, ScannerEntryPoint::kSmartActionsButton);

  EXPECT_EQ(
      GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSmartActionsButton),
      ScannerDisclaimerType::kNone);
  EXPECT_EQ(GetScannerDisclaimerType(prefs, ScannerEntryPoint::kSunfishSession),
            ScannerDisclaimerType::kReminder);
}

}  // namespace
}  // namespace ash
