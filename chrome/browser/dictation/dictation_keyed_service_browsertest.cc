// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/dictation_keyed_service.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/dictation/dictation_keyed_service_factory.h"
#include "chrome/browser/dictation/features.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chrome/test/base/platform_browser_test.h"
#include "content/public/test/browser_test.h"

namespace dictation {

class DictationKeyedServiceBrowserTest : public PlatformBrowserTest {
 public:
  DictationKeyedServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(kDictation);
  }
  ~DictationKeyedServiceBrowserTest() override = default;

  Profile* profile() { return chrome_test_utils::GetProfile(this); }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       CreatedForRegularProfile) {
  EXPECT_NE(DictationKeyedService::Get(profile()), nullptr);
}

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceBrowserTest,
                       NotCreatedForOTRProfile) {
  Profile* otr_profile =
      profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true);
  EXPECT_EQ(DictationKeyedService::Get(otr_profile), nullptr);
}

class DictationKeyedServiceDisabledBrowserTest
    : public DictationKeyedServiceBrowserTest {
 public:
  DictationKeyedServiceDisabledBrowserTest() {
    scoped_feature_list_.InitAndDisableFeature(kDictation);
  }
  ~DictationKeyedServiceDisabledBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(DictationKeyedServiceDisabledBrowserTest,
                       NotCreatedWhenDisabled) {
  EXPECT_EQ(DictationKeyedService::Get(profile()), nullptr);
}

}  // namespace dictation
