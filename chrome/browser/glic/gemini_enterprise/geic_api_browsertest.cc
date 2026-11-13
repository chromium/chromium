// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>

#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/gemini_enterprise/geic_enabling.h"
#include "chrome/browser/glic/gemini_enterprise/geic_fre.h"
#include "chrome/browser/glic/gemini_enterprise/geic_pref_names.h"
#include "chrome/browser/glic/glic_metrics.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_api_test.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "url/gurl.h"

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
    // Allows the non-allowlisted test server guest URL.
    command_line->AppendSwitch(geic::kGeicAllowAnyGuestURLSwitch);
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

// crbug.com/582296507: GE auth flows that used `window.open()` never completed
// in the side panel, since the guest gets no handle to the opened tab:
// `MaybeHandleGlicWindowCreation()` opens it in a new tab or popup instead.
// Pins that contract alongside the `openAuthTab()` replacement.
IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest,
                       testGeicWindowOpenReturnsNullButAuthTabIsTracked) {
  ASSERT_OK(OpenGlicForActiveTab());
  TabListInterface* tab_list = GetTabListInterface();
  ASSERT_EQ(tab_list->GetTabCount(), 1);

  // `window.open()` returns null to the guest, but still opens a tab.
  ExecuteJsTest();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return tab_list->GetTabCount() == 2; }));
  tabs::TabInterface* window_open_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(window_open_tab);
  EXPECT_EQ(window_open_tab->GetContents()->GetVisibleURL(),
            GURL("https://www.chromium.org"));
  const tabs::TabHandle window_open_handle = window_open_tab->GetHandle();

  // `openAuthTab()` opens a separate tab that Chrome tracks.
  ContinueJsTest();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return tab_list->GetTabCount() == 3; }));
  tabs::TabInterface* auth_tab = tab_list->GetActiveTab();
  ASSERT_TRUE(auth_tab);
  EXPECT_NE(auth_tab->GetHandle(), window_open_handle);
  EXPECT_EQ(auth_tab->GetContents()->GetVisibleURL(),
            GURL("https://accounts.google.com/signin"));

  // `closeAuthTab()` closes only the tracked tab, not the `window.open()` tab.
  ContinueJsTest();
  ASSERT_TRUE(
      base::test::RunUntil([&]() { return tab_list->GetTabCount() == 2; }));
  EXPECT_NE(tab_list->GetIndexOfTab(window_open_handle), -1);
}

// GEiC doesn't use the Chrome primary account: the GE web app handles sign-in,
// and there is no AuthController. Pausing Chrome sign-in leaves the GE guest
// showing, and the web client can still call the host API.
IN_PROC_BROWSER_TEST_F(GlicGeicApiBrowserTest, testSignInPauseStateGeic) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ExecuteJsTest();

  // Pause the sign-in.
  signin::SetInvalidRefreshTokenForPrimaryAccount(
      IdentityManagerFactory::GetForProfile(GetProfile()));

  // The web client calls the host API again after the pause.
  ContinueJsTest();
  ASSERT_OK(WaitUntilGuestIsShowing());
  EXPECT_TRUE(instance->host().IsWebClientConnected());
  EXPECT_TRUE(FindGlicGuestMainFrame());
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
// onboarding steps, but records provider-split onboarding metrics.
IN_PROC_BROWSER_TEST_F(GlicGeicFreApiBrowserTest,
                       testGeicSetOnboardingCompleted) {
  ASSERT_OK(OpenGlicForActiveTab());
  ExecuteJsTest();

  ASSERT_FALSE(GlicEnabling::HasConsentedForProfile(GetProfile()));
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  ContinueJsTest();

  EXPECT_TRUE(GlicEnabling::HasConsentedForProfile(GetProfile()));
  EXPECT_EQ(geic::GetFreStatus(GetProfile()),
            geic::prefs::GeicFreStatus::kCompleted);
  // The consumer FRE is untouched.
  EXPECT_EQ(GetProfile()->GetPrefs()->GetInteger(prefs::kGlicCompletedFre),
            std::to_underlying(prefs::FreStatus::kNotStarted));
  EXPECT_EQ(1,
            user_action_tester.GetActionCount("Glic.Onboarding.OptInAccept"));
  histogram_tester.ExpectUniqueSample("Glic.Fre.Accept.FlowSource",
                                      OptInFlow::kGlicFre, 1);

  // `OpenGlicForActiveTab()` opens the panel from the top Chrome button.
  histogram_tester.ExpectUniqueSample(
      "Glic.Enterprise.Fre.Accept.InvocationSource",
      mojom::InvocationSource::kTopChromeButton, 1);
  histogram_tester.ExpectTotalCount("Glic.Consumer.Fre.Accept.InvocationSource",
                                    0);
  histogram_tester.ExpectUniqueSample("Glic.Fre.Accept.InvocationSource",
                                      mojom::InvocationSource::kTopChromeButton,
                                      1);
}

}  // namespace
}  // namespace glic
