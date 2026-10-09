// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/universal_optout/features.h"
#include "components/universal_optout/prefs.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"

namespace {

// Global Privacy Control (GPC) in Chrome is controlled by three base::Features
// plus a profile pref:
//
//  * universal_optout::features::kUniversalOptOut and
//    universal_optout::features::kUniversalOptOutSettings: when both are
//    enabled, ChromeContentRendererClient turns on the GlobalPrivacyControlApi
//    Blink runtime feature (exposing `navigator.globalPrivacyControl`) and
//    chrome/browser/renderer_preferences_util.cc copies the
//    kUniversalOptOutEnabled pref into RendererPreferences.
//  * blink::features::kGlobalPrivacyControlApi: required by
//    blink::IsGlobalPrivacyControlFeatureAndSettingEnabled() for the API to
//    return true and for the Sec-GPC header to be sent. Enabling it also turns
//    on the GlobalPrivacyControlApi runtime feature (via
//    WebRuntimeFeatures::UpdateStatusFromBaseFeatures), so it exposes the API
//    on its own too.
//  * universal_optout::prefs::kUniversalOptOutEnabled: the user's setting.
//
// Each test case below describes one combination of these and the observable
// outcome in a page.
struct GlobalPrivacyControlTestCase {
  // Suffix used to name the parameterized test instance.
  const char* name;

  bool universal_opt_out;
  bool universal_opt_out_settings;
  // When false, blink::features::kGlobalPrivacyControlApi is left at its
  // default (disabled) state rather than explicitly disabled. An explicit
  // override would also force the Blink runtime feature off, which would hide
  // the fact that Chrome enables it from the kUniversalOptOut* features.
  bool gpc_api;
  bool pref_enabled;

  // Whether `navigator.globalPrivacyControl` is defined.
  bool expect_api_exposed;
  // Whether `navigator.globalPrivacyControl` is true and the Sec-GPC header is
  // sent.
  bool expect_signal;
};

constexpr GlobalPrivacyControlTestCase kTestCases[] = {
    {.name = "AllDisabled",
     .universal_opt_out = false,
     .universal_opt_out_settings = false,
     .gpc_api = false,
     .pref_enabled = true,
     .expect_api_exposed = false,
     .expect_signal = false},
    // kUniversalOptOut alone does nothing without kUniversalOptOutSettings.
    {.name = "UniversalOptOutOnly",
     .universal_opt_out = true,
     .universal_opt_out_settings = false,
     .gpc_api = false,
     .pref_enabled = true,
     .expect_api_exposed = false,
     .expect_signal = false},
    // kGlobalPrivacyControlApi alone exposes the API, but the pref is never
    // plumbed into RendererPreferences, so no signal is sent.
    {.name = "GpcApiOnly",
     .universal_opt_out = false,
     .universal_opt_out_settings = false,
     .gpc_api = true,
     .pref_enabled = true,
     .expect_api_exposed = true,
     .expect_signal = false},
    // Both Universal Opt-Out features expose the API, but without
    // kGlobalPrivacyControlApi the setting is ignored and no signal is sent.
    {.name = "UniversalOptOutAndSettings",
     .universal_opt_out = true,
     .universal_opt_out_settings = true,
     .gpc_api = false,
     .pref_enabled = true,
     .expect_api_exposed = true,
     .expect_signal = false},
    {.name = "UniversalOptOutAndGpcApi",
     .universal_opt_out = true,
     .universal_opt_out_settings = false,
     .gpc_api = true,
     .pref_enabled = true,
     .expect_api_exposed = true,
     .expect_signal = false},
    // All three features enabled: the pref decides.
    {.name = "AllEnabledPrefOff",
     .universal_opt_out = true,
     .universal_opt_out_settings = true,
     .gpc_api = true,
     .pref_enabled = false,
     .expect_api_exposed = true,
     .expect_signal = false},
    {.name = "AllEnabledPrefOn",
     .universal_opt_out = true,
     .universal_opt_out_settings = true,
     .gpc_api = true,
     .pref_enabled = true,
     .expect_api_exposed = true,
     .expect_signal = true},
};

class ChromeGlobalPrivacyControlTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<GlobalPrivacyControlTestCase> {
 public:
  ChromeGlobalPrivacyControlTest() {
    std::vector<base::test::FeatureRef> enabled_features;
    std::vector<base::test::FeatureRef> disabled_features;
    (GetParam().universal_opt_out ? enabled_features : disabled_features)
        .push_back(universal_optout::features::kUniversalOptOut);
    (GetParam().universal_opt_out_settings ? enabled_features
                                           : disabled_features)
        .push_back(universal_optout::features::kUniversalOptOutSettings);
    if (GetParam().gpc_api) {
      enabled_features.push_back(blink::features::kGlobalPrivacyControlApi);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }

 protected:
  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    SetUniversalOptOutPref(GetParam().pref_enabled);
    ASSERT_TRUE(embedded_test_server()->Start());
  }

  void SetUniversalOptOutPref(bool enabled) {
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        universal_optout::prefs::kUniversalOptOutEnabled, enabled);
  }

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  bool IsApiExposed() {
    return content::EvalJs(GetWebContents(),
                           "'globalPrivacyControl' in navigator")
        .ExtractBool();
  }

  bool GetApiValue() {
    // Compare against true so that an undefined property reads as false.
    return content::EvalJs(GetWebContents(),
                           "navigator.globalPrivacyControl === true")
        .ExtractBool();
  }

  std::string FetchSecGpcHeader() {
    return content::EvalJs(GetWebContents(),
                           "fetch('/echoheader?Sec-GPC').then(r => r.text());")
        .ExtractString();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Loads a page and checks the API exposure, the API value, and the Sec-GPC
// header on both the navigation and a subresource fetch.
IN_PROC_BROWSER_TEST_P(ChromeGlobalPrivacyControlTest, LoadPage) {
  const GlobalPrivacyControlTestCase& test_case = GetParam();
  const std::string expected_header = test_case.expect_signal ? "1" : "None";

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/echoheader?Sec-GPC")));

  EXPECT_EQ(expected_header,
            content::EvalJs(GetWebContents(), "document.body.innerText;"));
  EXPECT_EQ(test_case.expect_api_exposed, IsApiExposed());
  EXPECT_EQ(test_case.expect_signal, GetApiValue());
  EXPECT_EQ(expected_header, FetchSecGpcHeader());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ChromeGlobalPrivacyControlTest,
    ::testing::ValuesIn(kTestCases),
    [](const ::testing::TestParamInfo<GlobalPrivacyControlTestCase>& info) {
      return info.param.name;
    });

// Checks that, with every feature enabled, toggling the setting is reflected
// in the renderer without a reload.
class ChromeGlobalPrivacyControlAllEnabledTest : public InProcessBrowserTest {
 public:
  ChromeGlobalPrivacyControlAllEnabledTest() {
    scoped_feature_list_.InitWithFeatures(
        {universal_optout::features::kUniversalOptOut,
         universal_optout::features::kUniversalOptOutSettings,
         blink::features::kGlobalPrivacyControlApi},
        {});
  }

 protected:
  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  void SetUniversalOptOutPref(bool enabled) {
    browser()->GetProfile()->GetPrefs()->SetBoolean(
        universal_optout::prefs::kUniversalOptOutEnabled, enabled);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ChromeGlobalPrivacyControlAllEnabledTest,
                       PrefChangeIsReflectedImmediately) {
  ASSERT_TRUE(embedded_test_server()->Start());
  SetUniversalOptOutPref(true);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/empty.html")));
  EXPECT_EQ(true, content::EvalJs(GetWebContents(),
                                  "navigator.globalPrivacyControl"));
  EXPECT_EQ("1", content::EvalJs(
                     GetWebContents(),
                     "fetch('/echoheader?Sec-GPC').then(r => r.text());"));

  SetUniversalOptOutPref(false);
  EXPECT_EQ(false, content::EvalJs(GetWebContents(),
                                   "navigator.globalPrivacyControl"));
  EXPECT_EQ("None", content::EvalJs(
                        GetWebContents(),
                        "fetch('/echoheader?Sec-GPC').then(r => r.text());"));
}

}  // namespace
