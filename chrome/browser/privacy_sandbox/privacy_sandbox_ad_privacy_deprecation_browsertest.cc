// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/privacy_sandbox_features.h"
#include "components/privacy_sandbox/privacy_sandbox_prefs.h"
#include "content/public/test/browser_test.h"

namespace privacy_sandbox {

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

}  // namespace privacy_sandbox
