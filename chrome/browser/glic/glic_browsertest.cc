// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/run_until.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/glic_test_util.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
#include "ui/base/window_open_disposition.h"

namespace glic {
namespace {

class GlicBrowserTest : public InProcessBrowserTest {
 public:
  GlicBrowserTest() = default;
  GlicBrowserTest(const GlicBrowserTest&) = delete;
  GlicBrowserTest& operator=(const GlicBrowserTest&) = delete;

  ~GlicBrowserTest() override = default;

  void SetUp() override {
    InitializeFeatureList();
    InProcessBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Load blank page in glic guest view
    command_line->AppendSwitchASCII(::switches::kGlicGuestURL, "about:blank");
  }

 protected:
  virtual void InitializeFeatureList() {
    scoped_feature_list_.InitWithFeatures(
        {}, {features::kGlicTrustFirstOnboarding});
  }

 private:
  GlicTestEnvironment glic_test_environment_{{.fre_status = std::nullopt}};
  base::test::ScopedFeatureList scoped_feature_list_;
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

IN_PROC_BROWSER_TEST_F(GlicBrowserTest, TabHostIsRemovedWhenTabClosed) {
  auto* profile = browser()->profile();
  auto* glic_service = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  ASSERT_TRUE(glic_service);

  size_t initial_hosts_count =
      glic_service->host_manager().GetAllHosts().size();

  // Open chrome://glic in a new tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUIGlicURL),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  size_t hosts_count_after_open =
      glic_service->host_manager().GetAllHosts().size();
  EXPECT_EQ(hosts_count_after_open, initial_hosts_count + 1);

  tabs::TabInterface* glic_tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_TRUE(glic_tab);

  Host* tab_host =
      glic_service->host_manager().FindHostForTabForTesting(*glic_tab);
  ASSERT_TRUE(tab_host);

  base::WeakPtr<Host> tab_host_weak = tab_host->GetWeakPtr();
  ASSERT_TRUE(tab_host_weak);

  browser()->tab_strip_model()->CloseWebContentsAt(
      browser()->tab_strip_model()->active_index(), TabCloseTypes::CLOSE_NONE);

  // Wait for the tab close to finish tearing down the tab Host.
  ASSERT_TRUE(base::test::RunUntil([&]() { return !tab_host_weak; }));

  EXPECT_EQ(glic_service->host_manager().GetAllHosts().size(),
            initial_hosts_count);
}

}  // namespace
}  // namespace glic
