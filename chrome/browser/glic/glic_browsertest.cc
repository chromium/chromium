// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/raw_ptr.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/run_until.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
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
  }

 private:
  GlicTestEnvironment glic_test_environment_{{.fre_status = std::nullopt}};
};

class SharedGlicBrowserTest : public glic::GlicBrowserTest {
 public:
  // Set up the Glic UI for testing. This runs after the browser is launched.
  void SetUpOnMainThread() override {
    glic::GlicBrowserTest::SetUpOnMainThread();
    auto* profile = GetProfile();
    SetFRECompletion(profile, prefs::FreStatus::kCompleted);

    ASSERT_OK_AND_ASSIGN(auto* instance, OpenGlicForActiveTab());
    web_contents_ = instance->host().webui_contents();
    ASSERT_TRUE(web_contents_);
    ASSERT_TRUE(WaitForWebUiState(mojom::WebUiState::kReady).has_value());
  }
  // Clear the raw_ptr before the test environment destroys the WebContents,
  // preventing a dangling pointer error.
  void TearDownOnMainThread() override {
    web_contents_ = nullptr;
    glic::GlicBrowserTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<content::WebContents> web_contents_ = nullptr;
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

// This test verifies that focus is correctly trapped and forwarded to the
// guest panel's webview when the guest panel is visible.
// This simulates the behavior expected by the focus listener in
// glic_app_controller.ts.
IN_PROC_BROWSER_TEST_F(SharedGlicBrowserTest, FocusTrappingGuestPanel) {
  // Execute script to simulate focus trapping.
  std::string script = R"(
    (() => {
      // Simulate focus on body
      document.body.focus();
      // Trigger the focus event listener
      window.dispatchEvent(new Event('focus'));
      const webview = document.querySelector('#webviewContainer webview');
      if (document.activeElement === webview) {
        return 'success';
      }
      return 'activeElement is ' + document.activeElement.tagName;
    })()
  )";

  EXPECT_EQ("success", content::EvalJs(web_contents_, script));
}

}  // namespace
}  // namespace glic
