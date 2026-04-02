// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_cueing/contextual_cueing_service.h"

#include "chrome/browser/contextual_cueing/contextual_cueing_service_factory.h"
#include "chrome/browser/contextual_cueing/features.h"
#include "chrome/browser/extensions/keyed_services/browser_context_keyed_service_factories.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/buildflags.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"

namespace contextual_cueing {

class TestCueTarget : public CueTarget {
 public:
  bool eligible = true;
  CueActionData click_data = std::monostate();

  ~TestCueTarget() override = default;
  bool IsEligible() const override { return eligible; }
  void OnClick(CueActionData data) override { click_data = std::move(data); }
};

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

IN_PROC_BROWSER_TEST_F(ContextualCueingServiceV2BrowserTestCCFlag, OnClick) {
  auto* service =
      ContextualCueingServiceFactory::GetForProfile(browser()->profile());

  auto target = std::make_unique<TestCueTarget>();
  TestCueTarget* target_raw = target.get();
  service->RegisterCueTarget(CueTargetType::kGlic, std::move(target));

  ASSERT_TRUE(std::holds_alternative<std::monostate>(target_raw->click_data));

  service->OnClick(CueTargetType::kGlic, GlicCueActionData{.prompt = "asdf"});
  ASSERT_TRUE(
      std::holds_alternative<GlicCueActionData>(target_raw->click_data));
  EXPECT_EQ("asdf", std::get<GlicCueActionData>(target_raw->click_data).prompt);
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
