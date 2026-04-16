// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/contextual_cueing/test_cue_target.h"
#include "chrome/browser/extensions/keyed_services/browser_context_keyed_service_factories.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace contextual_cueing {

class ContextualCueingServiceV2BrowserTest : public InProcessBrowserTest {
 public:
  void SetUp() override {
    ContextualCueingServiceFactory::GetInstance();
    InProcessBrowserTest::SetUp();
  }
};

class ContextualCueingServiceV2BrowserTestCCFlag
    : public ContextualCueingServiceV2BrowserTest {
 public:
  ContextualCueingServiceV2BrowserTestCCFlag() {
    scoped_feature_list_.InitAndEnableFeature(kContextualCueingV2);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceV2BrowserTestCCFlag,
                       ServiceSpawnsWithCCFlag) {
  EXPECT_NE(nullptr, ContextualCueingServiceFactory::GetForProfile(
                         browser()->profile()));
}

class ContextualCueingServiceV2BrowserTestDisabledFeatures
    : public ContextualCueingServiceV2BrowserTest {
 public:
  ContextualCueingServiceV2BrowserTestDisabledFeatures() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{}, {kContextualCueingV2});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceV2BrowserTestDisabledFeatures,
                       NullServiceWithDisabledFeatures) {
  EXPECT_EQ(nullptr, ContextualCueingServiceFactory::GetForProfile(
                         browser()->profile()));
}

}  // namespace contextual_cueing
