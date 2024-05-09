// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/facilitated_payments/ui/chrome_facilitated_payments_client.h"

#include <memory>

#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/optimization_guide/core/optimization_guide_decider.h"
#include "testing/gmock/include/gmock/gmock.h"

class MockOptimizationGuideDecider
    : public optimization_guide::OptimizationGuideDecider {
 public:
  MOCK_METHOD(void,
              RegisterOptimizationTypes,
              (const std::vector<optimization_guide::proto::OptimizationType>&),
              (override));
  MOCK_METHOD(void,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationGuideDecisionCallback),
              (override));
  MOCK_METHOD(optimization_guide::OptimizationGuideDecision,
              CanApplyOptimization,
              (const GURL&,
               optimization_guide::proto::OptimizationType,
               optimization_guide::OptimizationMetadata*),
              (override));
  MOCK_METHOD(
      void,
      CanApplyOptimizationOnDemand,
      (const std::vector<GURL>&,
       const base::flat_set<optimization_guide::proto::OptimizationType>&,
       optimization_guide::proto::RequestContext,
       optimization_guide::OnDemandOptimizationGuideDecisionRepeatingCallback,
       std::optional<optimization_guide::proto::RequestContextMetadata>
           request_context_metadata),
      (override));
};

using ChromeFacilitatedPaymentsClientTest = ChromeRenderViewHostTestHarness;

TEST_F(ChromeFacilitatedPaymentsClientTest,
       GetFacilitatedPaymentsNetworkInterface) {
  MockOptimizationGuideDecider optimization_guide_decider;
  auto client = std::make_unique<ChromeFacilitatedPaymentsClient>(
      web_contents(), &optimization_guide_decider);

  EXPECT_NE(nullptr, client->GetFacilitatedPaymentsNetworkInterface());
}
