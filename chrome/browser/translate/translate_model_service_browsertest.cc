// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_model_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/translate/core/common/translate_util.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

using TranslateModelServiceDisabledBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(TranslateModelServiceDisabledBrowserTest,
                       TranslateModelServiceDisabled) {
  EXPECT_FALSE(
      TranslateModelServiceFactory::GetForProfile(browser()->profile()));
}

class TranslateModelServiceBrowserTest
    : public TranslateModelServiceDisabledBrowserTest {
 public:
  TranslateModelServiceBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        translate::kTFLiteLanguageDetectionEnabled);
  }

  ~TranslateModelServiceBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       TranslateModelServiceEnabled) {
  EXPECT_TRUE(
      TranslateModelServiceFactory::GetForProfile(browser()->profile()));
}

IN_PROC_BROWSER_TEST_F(TranslateModelServiceBrowserTest,
                       TranslateModelServiceEnabled_OffTheRecord) {
  EXPECT_TRUE(TranslateModelServiceFactory::GetForProfile(
      browser()->profile()->GetPrimaryOTRProfile()));
}
