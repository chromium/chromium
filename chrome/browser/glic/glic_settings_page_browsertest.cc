// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Tests which AI settings sections and subpages are shown for the consumer
// Gemini in Chrome (Glic) and Gemini Enterprise in Chrome (GEiC) providers.

#include <optional>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/glic_settings_util.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/public/glic_enabling.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_provider_spec.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_pages.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/reload_type.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace glic {
namespace {

constexpr char kGeicGuestUrl[] = "https://business.gemini.google/side-panel";

std::vector<base::test::FeatureRef> GetEnabledFeatures(bool geic,
                                                       bool geic_fre) {
  std::vector<base::test::FeatureRef> enabled =
      GetDefaultEnabledGlicTestFeatures();
  if (geic) {
    enabled.push_back(features::kGeic);
  }
  if (geic_fre) {
    enabled.push_back(features::kGeicFre);
  }
  return enabled;
}

std::vector<base::test::FeatureRef> GetDisabledFeatures(bool geic,
                                                        bool geic_fre) {
  std::vector<base::test::FeatureRef> disabled =
      GetDefaultDisabledGlicTestFeatures();
  if (!geic) {
    disabled.push_back(features::kGeic);
  }
  if (!geic_fre) {
    disabled.push_back(features::kGeicFre);
  }
  return disabled;
}

// Base fixture. The profile is latched to GEiC if `geic`, and to consumer
// Gemini otherwise.
class GlicSettingsPageBrowserTestBase : public InProcessBrowserTest {
 public:
  // `fre_status` is passed to `GlicTestEnvironmentConfig`. It sets the FRE of
  // the profile's provider, so it is the GEiC FRE for GEiC profiles.
  GlicSettingsPageBrowserTestBase(
      bool geic,
      bool geic_fre,
      std::optional<prefs::FreStatus> fre_status = prefs::FreStatus::kCompleted)
      : geic_(geic),
        glic_test_env_({.fre_status = fre_status},
                       GetEnabledFeatures(geic, geic_fre),
                       GetDisabledFeatures(geic, geic_fre)) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpCommandLine(command_line);
    if (geic_) {
      command_line->AppendSwitchASCII(::switches::kGeicGuestURL, kGeicGuestUrl);
    }
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(service());
    ASSERT_EQ(
        service()->provider_spec().GetSettingsSectionId(),
        geic_ ? kGeminiEnterpriseSettingsSectionId : kGeminiSettingsSectionId);
  }

 protected:
  GlicKeyedService* service() {
    return GlicKeyedService::Get(browser()->GetProfile());
  }

  content::WebContents* active_web_contents() {
    return browser()->GetTabStripModel()->GetActiveWebContents();
  }

  // Navigates the active tab to chrome://settings and waits for it to load.
  void NavigateToSettings() {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(chrome::kChromeUISettingsURL)));
    ASSERT_TRUE(content::ExecJs(active_web_contents(),
                                "customElements.whenDefined('settings-ui');"));
  }

  // Returns the string `key` from the settings page's loadTimeData.
  std::string GetLoadTimeString(const std::string& key) {
    return content::EvalJs(
               active_web_contents(),
               content::JsReplace(
                   "import('chrome://resources/js/load_time_data.js')"
                   ".then(m => m.loadTimeData.getString($1))",
                   key))
        .ExtractString();
  }

  // Returns the boolean `key` from the settings page's loadTimeData.
  bool GetLoadTimeBoolean(const std::string& key) {
    return content::EvalJs(
               active_web_contents(),
               content::JsReplace(
                   "import('chrome://resources/js/load_time_data.js')"
                   ".then(m => m.loadTimeData.getBoolean($1))",
                   key))
        .ExtractBool();
  }

  const bool geic_;

 private:
  GlicTestEnvironment glic_test_env_;
};

class GlicSettingsPageBrowserTest : public GlicSettingsPageBrowserTestBase,
                                    public testing::WithParamInterface<bool> {
 public:
  GlicSettingsPageBrowserTest()
      : GlicSettingsPageBrowserTestBase(/*geic=*/GetParam(),
                                        /*geic_fre=*/false) {}

  static std::string PrintParam(const testing::TestParamInfo<bool>& info) {
    return info.param ? "Geic" : "Gemini";
  }
};

// The AI page shows the provider's section: Gemini or Gemini Enterprise.
IN_PROC_BROWSER_TEST_P(GlicSettingsPageBrowserTest, ShowsProviderSection) {
  NavigateToSettings();
  EXPECT_EQ(
      GetLoadTimeString("aiSettingsSection"),
      geic_ ? kGeminiEnterpriseSettingsSectionId : kGeminiSettingsSectionId);
  EXPECT_TRUE(GetLoadTimeBoolean("showAiPage"));
}

// OpenGlicSettingsPage() opens the provider's settings subpage.
IN_PROC_BROWSER_TEST_P(GlicSettingsPageBrowserTest,
                       OpenGlicSettingsPageOpensProviderSubpage) {
  const GURL expected_url =
      chrome::GetSettingsUrl(geic_ ? chrome::kGlicEnterpriseSettingsSubpage
                                   : chrome::kGlicSettingsSubpage);

  content::TestNavigationObserver navigation_observer(expected_url);
  navigation_observer.WatchExistingWebContents();
  navigation_observer.StartWatchingNewWebContents();

  OpenGlicSettingsPage(browser()->GetProfile());

  navigation_observer.Wait();
  EXPECT_EQ(active_web_contents()->GetLastCommittedURL(), expected_url);
}

INSTANTIATE_TEST_SUITE_P(,
                         GlicSettingsPageBrowserTest,
                         testing::Bool(),
                         &GlicSettingsPageBrowserTest::PrintParam);

// The provider's section stays hidden until the provider's FRE is completed.
// For GEiC, this requires `features::kGeicFre`.
class GlicSettingsPageFreBrowserTest
    : public GlicSettingsPageBrowserTestBase,
      public testing::WithParamInterface<bool> {
 public:
  GlicSettingsPageFreBrowserTest()
      : GlicSettingsPageBrowserTestBase(/*geic=*/GetParam(),
                                        /*geic_fre=*/GetParam(),
                                        /*fre_status=*/std::nullopt) {}
};

IN_PROC_BROWSER_TEST_P(GlicSettingsPageFreBrowserTest,
                       HidesSectionUntilFreCompleted) {
  ASSERT_FALSE(service()->enabling().HasConsented());

  NavigateToSettings();
  EXPECT_EQ(GetLoadTimeString("aiSettingsSection"), "");

  // Completing the provider's FRE updates the settings page state, which
  // takes effect on the next page load.
  service()->enabling().SetCompletedFre(prefs::FreStatus::kCompleted);
  ASSERT_TRUE(service()->enabling().HasConsented());

  content::TestNavigationObserver reload_observer(active_web_contents());
  active_web_contents()->GetController().Reload(content::ReloadType::NORMAL,
                                                /*check_for_repost=*/false);
  reload_observer.Wait();
  ASSERT_TRUE(content::ExecJs(active_web_contents(),
                              "customElements.whenDefined('settings-ui');"));

  EXPECT_EQ(
      GetLoadTimeString("aiSettingsSection"),
      geic_ ? kGeminiEnterpriseSettingsSectionId : kGeminiSettingsSectionId);
  EXPECT_TRUE(GetLoadTimeBoolean("showAiPage"));
}

INSTANTIATE_TEST_SUITE_P(,
                         GlicSettingsPageFreBrowserTest,
                         testing::Bool(),
                         &GlicSettingsPageBrowserTest::PrintParam);

// Without `features::kGeicFre`, GEiC profiles are consented without an FRE, so
// the Gemini Enterprise section is shown before any FRE.
class GlicSettingsPageGeicWithoutFreBrowserTest
    : public GlicSettingsPageBrowserTestBase {
 public:
  GlicSettingsPageGeicWithoutFreBrowserTest()
      : GlicSettingsPageBrowserTestBase(/*geic=*/true,
                                        /*geic_fre=*/false,
                                        /*fre_status=*/std::nullopt) {}
};

IN_PROC_BROWSER_TEST_F(GlicSettingsPageGeicWithoutFreBrowserTest,
                       ShowsSectionWithoutFre) {
  ASSERT_TRUE(service()->enabling().HasConsented());

  NavigateToSettings();
  EXPECT_EQ(GetLoadTimeString("aiSettingsSection"),
            kGeminiEnterpriseSettingsSectionId);
  EXPECT_TRUE(GetLoadTimeBoolean("showAiPage"));
}

}  // namespace
}  // namespace glic
