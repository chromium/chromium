// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
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
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1TopicsEnabled, true);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1FledgeEnabled, true);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdPrivacyDeprecationTest,
                       PrefsSetToFalse) {
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1TopicsEnabled));
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1FledgeEnabled));
  EXPECT_FALSE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1AdMeasurementEnabled));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdPrivacyDeprecationTest,
                       AttributionInternalsWebUINull) {
  GURL kUrl("chrome://attribution-internals");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_TRUE(web_contents->GetPrimaryMainFrame()->IsErrorDocument());
  EXPECT_EQ(web_contents->GetWebUI(), nullptr);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdPrivacyDeprecationTest,
                       SettingsRoutesRedirect) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
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
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1TopicsEnabled, true);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1FledgeEnabled, true);
  browser()->profile()->GetPrefs()->SetBoolean(
      prefs::kPrivacySandboxM1AdMeasurementEnabled, true);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdPrivacyDeprecationDisabledTest,
                       PrefsNotSetToFalse) {
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1TopicsEnabled));
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1FledgeEnabled));
  EXPECT_TRUE(browser()->profile()->GetPrefs()->GetBoolean(
      prefs::kPrivacySandboxM1AdMeasurementEnabled));
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdPrivacyDeprecationDisabledTest,
                       AttributionInternalsWebUINotNull) {
  GURL kUrl("chrome://attribution-internals");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kUrl));
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_FALSE(web_contents->GetPrimaryMainFrame()->IsErrorDocument());
  EXPECT_NE(web_contents->GetWebUI(), nullptr);
}

IN_PROC_BROWSER_TEST_F(PrivacySandboxAdPrivacyDeprecationDisabledTest,
                       SettingsRoutesDoNotRedirect) {
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  for (const char* url_string : kAdPrivacyUrls) {
    GURL url(url_string);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
    EXPECT_EQ(web_contents->GetLastCommittedURL(), url);
  }
}

}  // namespace privacy_sandbox
