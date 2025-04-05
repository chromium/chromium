// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_features.h"
#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/extensions/keyed_services/browser_context_keyed_service_factories.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/test/browser_test.h"

namespace contextual_cueing {

class ContextualCueingServiceBrowserTest : public InProcessBrowserTest {
 public:
  ContextualCueingServiceBrowserTest() = default;

  void SetUp() override {
    ContextualCueingServiceFactory::GetInstance();
    InProcessBrowserTest::SetUp();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
};

class ContextualCueingServiceBrowserTestZSSFlag
    : public ContextualCueingServiceBrowserTest {
 public:
  ContextualCueingServiceBrowserTestZSSFlag() {
    scoped_feature_list_.InitAndEnableFeature(kGlicZeroStateSuggestions);
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestZSSFlag,
                       ServiceSpawnsWithZSSFlag) {
  EXPECT_NE(nullptr, ContextualCueingServiceFactory::GetForProfile(
                         browser()->profile()));
}

class ContextualCueingServiceBrowserTestCCFlag
    : public ContextualCueingServiceBrowserTest {
 public:
  ContextualCueingServiceBrowserTestCCFlag() {
    scoped_feature_list_.InitAndEnableFeature(kContextualCueing);
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestCCFlag,
                       ServiceSpawnsWithCCFlag) {
  EXPECT_NE(nullptr, ContextualCueingServiceFactory::GetForProfile(
                         browser()->profile()));
}

class ContextualCueingServiceBrowserTestDisabledFeatures
    : public ContextualCueingServiceBrowserTest {
 public:
  ContextualCueingServiceBrowserTestDisabledFeatures() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        {kContextualCueing, kGlicZeroStateSuggestions});
  }
};

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceBrowserTestDisabledFeatures,
                       NullServiceWithDisabledFeatures) {
  EXPECT_EQ(nullptr, ContextualCueingServiceFactory::GetForProfile(
                         browser()->profile()));
}

}  // namespace contextual_cueing
