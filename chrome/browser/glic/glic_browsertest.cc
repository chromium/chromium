// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/ui_features.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class GlicBrowserTest : public InProcessBrowserTest {
 public:
  GlicBrowserTest() {
  }
  GlicBrowserTest(const GlicBrowserTest&) = delete;
  GlicBrowserTest& operator=(const GlicBrowserTest&) = delete;

  ~GlicBrowserTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Load blank page in glic guest view
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL, "about:blank");
  }

 private:
  GlicTestEnvironment glic_test_environment_{{.fre_status = std::nullopt}};
};

// Ensure basic incognito window doesn't cause a crash. Simply opens an
// incognito window and navigates, test passes if it doesn't crash.
IN_PROC_BROWSER_TEST_F(GlicBrowserTest, IncognitoModeCrash) {
  Browser* incognito_browser = CreateIncognitoBrowser();

  ASSERT_TRUE(
      ui_test_utils::NavigateToURL(incognito_browser, GURL("about:blank")));
}

IN_PROC_BROWSER_TEST_F(GlicBrowserTest, PausedProfileIsNotReady) {
  // Signin and check that Glic is enabled.
  auto* profile = browser()->profile();
  auto* const identity_manager = IdentityManagerFactory::GetForProfile(profile);

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));

  // False until FRE is completed.
  ASSERT_FALSE(GlicEnabling::IsReadyForProfile(profile));
  SetFRECompletion(profile, prefs::FreStatus::kCompleted);
  ASSERT_TRUE(GlicEnabling::IsReadyForProfile(profile));

  signin::SetInvalidRefreshTokenForPrimaryAccount(identity_manager);

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));
  ASSERT_FALSE(GlicEnabling::IsReadyForProfile(profile));
}

IN_PROC_BROWSER_TEST_F(GlicBrowserTest, GlicEnablingDismissed) {
  // Signin and check that Glic is enabled.
  auto* profile = browser()->profile();

  ASSERT_TRUE(GlicEnabling::IsEnabledForProfile(profile));

  // False until FRE is shown.
  ASSERT_FALSE(GlicEnabling::DidDismissForProfile(profile));

  // Simulate user shown FRE and dismissed.
  SetFRECompletion(profile, prefs::FreStatus::kIncomplete);
  ASSERT_TRUE(GlicEnabling::DidDismissForProfile(profile));

  // Simulate user shown FRE again and accepted.
  SetFRECompletion(profile, prefs::FreStatus::kCompleted);
  ASSERT_FALSE(GlicEnabling::DidDismissForProfile(profile));
}

}  // namespace
}  // namespace glic
