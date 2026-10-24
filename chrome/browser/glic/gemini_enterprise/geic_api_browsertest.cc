// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/gemini_enterprise/geic_fre.h"
#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"

namespace glic {
namespace {

class GlicGeicApiBrowserTest : public GlicApiBrowserTest {
 public:
  GlicGeicApiBrowserTest()
      : GlicApiBrowserTest(GlicTestJsPath("./geic_api_browsertest.js")) {
    feature_list_.InitAndEnableFeature(features::kGeic);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicApiBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(geic::kGeicGuestURLSwitch,
                                    GetGuestURL().spec());
    command_line->AppendSwitch(::switches::kGlicDev);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest, testGeicSignInTab) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest, testGeicAuthTab) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();
}

class GlicGeicFreApiBrowserTest : public GlicGeicApiBrowserTest {
 public:
  GlicGeicFreApiBrowserTest() {
    geic_fre_feature_list_.InitAndEnableFeature(features::kGeicFre);
    // Start with neither FRE completed. The GEiC FRE ignores this, since it
    // can only be completed.
    glic_test_environment().SetFreStatusForNewProfiles(
        prefs::FreStatus::kNotStarted);
  }

 private:
  base::test::ScopedFeatureList geic_fre_feature_list_;
};

// The onboarding API completes the GEiC FRE and skips the consumer-only
// onboarding steps.
IN_PROC_BROWSER_TEST_F(GlicGeicFreApiBrowserTest,
                       testGeicSetOnboardingCompleted) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  ASSERT_FALSE(GlicEnabling::HasConsentedForProfile(GetProfile()));
  base::UserActionTester user_action_tester;

  ContinueJsTest();

  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(GetProfile()));
  EXPECT_EQ(geic::GetFreStatus(GetProfile()),
            geic::prefs::GeicFreStatus::kCompleted);
  // The consumer FRE and onboarding metrics are untouched.
  EXPECT_EQ(GetProfile()->GetPrefs()->GetInteger(prefs::kGlicCompletedFre),
            std::to_underlying(prefs::FreStatus::kNotStarted));
  EXPECT_EQ(0,
            user_action_tester.GetActionCount("Glic.Onboarding.OptInAccept"));
}

}  // namespace
}  // namespace glic
