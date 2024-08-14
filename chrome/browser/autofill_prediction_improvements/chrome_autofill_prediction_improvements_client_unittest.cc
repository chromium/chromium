// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_prediction_improvements/chrome_autofill_prediction_improvements_client.h"

#include "base/functional/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/optimization_guide/mock_optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/optimization_guide/proto/features/common_quality_data.pb.h"
#include "components/user_annotations/user_annotations_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

std::unique_ptr<KeyedService> CreateOptimizationGuideKeyedService(
    content::BrowserContext* context) {
  return std::make_unique<MockOptimizationGuideKeyedService>();
}

std::unique_ptr<KeyedService> CreateUserAnnotationsServiceFactory(
    content::BrowserContext* context) {
  return std::make_unique<user_annotations::UserAnnotationsService>();
}

}  // namespace

class ChromeAutofillPredictionImprovementsClientTest
    : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    ChromeAutofillPredictionImprovementsClient::CreateForWebContents(
        web_contents());
  }

  ChromeAutofillPredictionImprovementsClient* client() {
    return ChromeAutofillPredictionImprovementsClient::FromWebContents(
        web_contents());
  }

  TestingProfile::TestingFactories GetTestingFactories() const override {
    return {TestingProfile::TestingFactory{
                OptimizationGuideKeyedServiceFactory::GetInstance(),
                base::BindRepeating(&CreateOptimizationGuideKeyedService)},
            TestingProfile::TestingFactory{
                UserAnnotationsServiceFactory::GetInstance(),
                base::BindRepeating(&CreateUserAnnotationsServiceFactory)}};
  }
};

TEST_F(ChromeAutofillPredictionImprovementsClientTest, GetAXTree) {
  base::MockCallback<autofill_prediction_improvements::
                         AutofillPredictionImprovementsClient::AXTreeCallback>
      callback;
  EXPECT_CALL(callback, Run);
  client()->GetAXTree(callback.Get());
}
