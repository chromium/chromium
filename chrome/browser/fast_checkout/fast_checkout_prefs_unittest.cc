// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_prefs.h"

#include "base/command_line.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const char kAutofillAssistantForceOnboarding[] =
    "autofill-assistant-force-onboarding";

const char kFastCheckoutOnboardingDeclined[] =
    "fast_checkout.onboarding_declined";
}  // namespace

class FastCheckoutPrefsTest : public ::testing::Test {
 public:
  void SetUp() override {
    pref_service_ = std::make_unique<TestingPrefServiceSimple>();
    fast_checkout_prefs_ =
        std::make_unique<FastCheckoutPrefs>(pref_service_.get());
    FastCheckoutPrefs::RegisterProfilePrefs(pref_service_->registry());
  }

  TestingPrefServiceSimple* pref_service() { return pref_service_.get(); }

  FastCheckoutPrefs* fast_checkout_prefs() {
    return fast_checkout_prefs_.get();
  }

  void EnableAutofillAssistantForceOnboarding() {
    SetAutofillAssistantForceOnboarding(true);
  }

  void DisableAutofillAssistantForceOnboarding() {
    SetAutofillAssistantForceOnboarding(false);
  }

 private:
  std::unique_ptr<TestingPrefServiceSimple> pref_service_;
  std::unique_ptr<FastCheckoutPrefs> fast_checkout_prefs_;

  void SetAutofillAssistantForceOnboarding(bool force_onboarding) {
    base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
    if (command_line->HasSwitch(kAutofillAssistantForceOnboarding)) {
      command_line->RemoveSwitch(kAutofillAssistantForceOnboarding);
    }
    command_line->AppendSwitchASCII(kAutofillAssistantForceOnboarding,
                                    force_onboarding ? "true" : "false");
  }
};

TEST_F(FastCheckoutPrefsTest, IsOnboardingDeclined_WasDeclined_ReturnsTrue) {
  DisableAutofillAssistantForceOnboarding();
  pref_service()->SetBoolean(kFastCheckoutOnboardingDeclined, true);
  EXPECT_TRUE(fast_checkout_prefs()->IsOnboardingDeclined());
}

TEST_F(FastCheckoutPrefsTest, IsOnboardingDeclined_NotDeclined_ReturnsFalse) {
  DisableAutofillAssistantForceOnboarding();
  pref_service()->SetBoolean(kFastCheckoutOnboardingDeclined, false);
  EXPECT_FALSE(fast_checkout_prefs()->IsOnboardingDeclined());
}

TEST_F(FastCheckoutPrefsTest,
       IsOnboardingDeclined_WasDeclinedForceOnboarding_ReturnsFalse) {
  EnableAutofillAssistantForceOnboarding();
  pref_service()->SetBoolean(kFastCheckoutOnboardingDeclined, true);
  EXPECT_FALSE(fast_checkout_prefs()->IsOnboardingDeclined());
}

TEST_F(FastCheckoutPrefsTest,
       IsOnboardingDeclined_NotDeclinedForceOnboarding_ReturnsFalse) {
  EnableAutofillAssistantForceOnboarding();
  pref_service()->SetBoolean(kFastCheckoutOnboardingDeclined, false);
  EXPECT_FALSE(fast_checkout_prefs()->IsOnboardingDeclined());
}

TEST_F(FastCheckoutPrefsTest, DeclineOnboarding_SetsPrefToTrue) {
  pref_service()->SetBoolean(kFastCheckoutOnboardingDeclined, false);
  EXPECT_FALSE(pref_service()->GetBoolean(kFastCheckoutOnboardingDeclined));

  fast_checkout_prefs()->DeclineOnboarding();

  EXPECT_TRUE(pref_service()->GetBoolean(kFastCheckoutOnboardingDeclined));
}
