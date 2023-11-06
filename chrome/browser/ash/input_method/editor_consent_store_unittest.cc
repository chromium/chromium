// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/editor_consent_store.h"

#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/input_method/editor_consent_enums.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash::input_method {
namespace {

class EditorConsentStoreDelegateForTesting
    : public EditorConsentStore::Delegate {
 public:
  EditorConsentStoreDelegateForTesting() {}
  EditorMode GetEditorMode() const override { return EditorMode::kBlocked; }
};

class EditorConsentStoreTest : public ::testing::Test {
 public:
  EditorConsentStoreTest() = default;
  ~EditorConsentStoreTest() override = default;

 private:
  content::BrowserTaskEnvironment task_environment_;
};

TEST_F(EditorConsentStoreTest,
       ReceivingDeclineResponseWillLeadToConsentDecline) {
  TestingProfile profile_;
  EditorConsentStoreDelegateForTesting editor_consent_delegate;
  EditorConsentStore store(profile_.GetPrefs(), &editor_consent_delegate);

  store.ProcessConsentAction(ConsentAction::kDeclined);

  EXPECT_EQ(store.GetConsentStatus(), ConsentStatus::kDeclined);
}

TEST_F(EditorConsentStoreTest,
       ReceivingApprovalResponseWillLeadToConsentApproval) {
  TestingProfile profile_;
  EditorConsentStoreDelegateForTesting editor_consent_delegate;
  EditorConsentStore store(profile_.GetPrefs(), &editor_consent_delegate);

  store.ProcessConsentAction(ConsentAction::kApproved);

  EXPECT_EQ(store.GetConsentStatus(), ConsentStatus::kApproved);
}

TEST_F(EditorConsentStoreTest,
       SwitchingOnSettingToggleWillResetConsentWhichWasPreviouslyDeclined) {
  TestingProfile profile_;
  EditorConsentStoreDelegateForTesting editor_consent_delegate;
  EditorConsentStore store(profile_.GetPrefs(), &editor_consent_delegate);

  store.ProcessConsentAction(ConsentAction::kDeclined);
  // Simulate a user action to switch on the orca toggle.
  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);

  EXPECT_EQ(store.GetConsentStatus(), ConsentStatus::kUnset);
}

TEST_F(EditorConsentStoreTest,
       DecliningThePromoCardWillSwitchOffFeatureToggle) {
  TestingProfile profile_;
  EditorConsentStoreDelegateForTesting editor_consent_delegate;
  EditorConsentStore store(profile_.GetPrefs(), &editor_consent_delegate);

  // Switch on the orca toggle in the setting page.
  profile_.GetPrefs()->SetBoolean(prefs::kOrcaEnabled, true);
  // Simulate a user action to explicitly decline the promo card.
  store.ProcessPromoCardAction(PromoCardAction::kDeclined);

  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(prefs::kOrcaEnabled));
}

}  // namespace
}  // namespace ash::input_method
