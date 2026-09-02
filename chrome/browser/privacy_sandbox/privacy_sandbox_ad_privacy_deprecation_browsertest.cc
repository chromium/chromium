// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/test/browser_test.h"

namespace privacy_sandbox {
namespace {

constexpr const char* kAdPrivacyUrls[] = {
    "chrome://settings/adPrivacy",
    "chrome://settings/adPrivacy/interests",
    "chrome://settings/adPrivacy/interests/manage",
    "chrome://settings/adPrivacy/sites",
    "chrome://settings/adPrivacy/measurement",
};

}  // namespace

class PrivacySandboxAdPrivacyDeprecationTest : public InProcessBrowserTest {
 public:
  PrivacySandboxAdPrivacyDeprecationTest() {
    feature_list_.InitAndEnableFeature(kPrivacySandboxAdPrivacyUxDeprecation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdPrivacyDeprecationTest,
                       PRE_PrefsSetToFalse) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1TopicsEnabled, true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1FledgeEnabled, true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdPrivacyDeprecationTest,
                       PrefsSetToFalse) {
  EXPECT_FALSE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1TopicsEnabled));
  EXPECT_FALSE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1FledgeEnabled));
  EXPECT_FALSE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1AdMeasurementEnabled));
}

#if BUILDFLAG(IS_LINUX)
#define MAYBE_SettingsRoutesRedirect DISABLED_SettingsRoutesRedirect
#else
#define MAYBE_SettingsRoutesRedirect SettingsRoutesRedirect
#endif
IN_PROC_BROWSER_TEST_F(PrivacySandboxAdPrivacyDeprecationTest,
                       MAYBE_SettingsRoutesRedirect) {
  content::WebContents* web_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  GURL base_settings_url("chrome://settings/");
  for (const char* url_string : kAdPrivacyUrls) {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL(url_string)));
    EXPECT_EQ(web_contents->GetLastCommittedURL(), base_settings_url);
  }
}

class PrivacySandboxAdPrivacyDeprecationDisabledTest
    : public InProcessBrowserTest {
 public:
  PrivacySandboxAdPrivacyDeprecationDisabledTest() {
    feature_list_.InitAndDisableFeature(kPrivacySandboxAdPrivacyUxDeprecation);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdPrivacyDeprecationDisabledTest,
                       PRE_PrefsNotSetToFalse) {
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1TopicsEnabled, true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1FledgeEnabled, true);
  browser()->GetProfile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdPrivacyDeprecationDisabledTest,
                       PrefsNotSetToFalse) {
  EXPECT_TRUE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1TopicsEnabled));
  EXPECT_TRUE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1FledgeEnabled));
  EXPECT_TRUE(browser()->GetProfile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1AdMeasurementEnabled));
}

}  // namespace privacy_sandbox
