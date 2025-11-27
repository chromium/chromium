// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"

#include <utility>

#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/decorators/page_aggregator.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "testing/gtest/include/gtest/gtest.h"

using performance_manager::policies::CanDiscardResult;
using performance_manager::policies::CannotDiscardReason;
using performance_manager::policies::DiscardEligibilityPolicy;
using DiscardReason =
    performance_manager::policies::DiscardEligibilityPolicy::DiscardReason;

namespace performance_manager::testing {

void MakePageNodeDiscardable(PageNodeImpl* page_node,
                             content::BrowserTaskEnvironment& task_env) {
  page_node->SetIsVisible(false);
  page_node->SetIsAudible(false);
  page_node->SetType(PageType::kTab);
  const auto kUrl = GURL("https://foo.com");
  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), 42, kUrl, "text/html",
      /*notification_permission_status=*/blink::mojom::PermissionStatus::ASK);
  (*page_node->main_frame_nodes().begin())
      ->OnNavigationCommitted(kUrl, url::Origin::Create(kUrl),
                              /*same_document=*/false,
                              /*is_served_from_back_forward_cache=*/false);
  task_env.FastForwardBy(base::Minutes(10));
  const auto* eligibility_policy =
      policies::DiscardEligibilityPolicy::GetFromGraph(page_node->graph());
  CHECK_EQ(eligibility_policy->CanDiscard(page_node, DiscardReason::URGENT),
           CanDiscardResult::kEligible);
  CHECK_EQ(eligibility_policy->CanDiscard(page_node, DiscardReason::PROACTIVE),
           CanDiscardResult::kEligible);
  CHECK_EQ(eligibility_policy->CanDiscard(page_node, DiscardReason::EXTERNAL),
           CanDiscardResult::kEligible);
  CHECK_EQ(eligibility_policy->CanDiscard(page_node, DiscardReason::SUGGESTED),
           CanDiscardResult::kEligible);
}

GraphTestHarnessWithDiscardablePage::GraphTestHarnessWithDiscardablePage()
    : GraphTestHarness(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

GraphTestHarnessWithDiscardablePage::~GraphTestHarnessWithDiscardablePage() =
    default;

void GraphTestHarnessWithDiscardablePage::SetUp() {
  // Some tests depends on the existence of the PageAggregator.
  GetGraphFeatures().EnablePageAggregator();

  GraphTestHarness::SetUp();

  // The discarding logic relies on the existence of the page live state data.
  graph()->PassToGraph(std::make_unique<PageLiveStateDecorator>());

  // The DiscardEligibilityPolicy usually keeps track of the relevant patterns
  // on profile creation and deletion. Since no profile is involved in this kind
  // of test, add an empty patterns list associated with the "empty string"
  // browser context ID, which is the one used by default for new PageNodes.
  auto eligibility_policy =
      std::make_unique<policies::DiscardEligibilityPolicy>();
  eligibility_policy->SetNoDiscardPatternsForProfile("", {});

  graph()->PassToGraph(std::move(eligibility_policy));

  // Create a PageNode and make it discardable.
  RecreateNodes();
}

void GraphTestHarnessWithDiscardablePage::TearDown() {
  main_frame_node_.reset();
  page_node_.reset();
  process_node_.reset();
  GraphTestHarness::TearDown();
}

void GraphTestHarnessWithDiscardablePage::RecreateNodes() {
  main_frame_node_.reset();
  page_node_.reset();
  process_node_.reset();

  process_node_ = CreateNode<performance_manager::ProcessNodeImpl>();
  page_node_ = CreateNode<performance_manager::PageNodeImpl>();
  main_frame_node_ =
      CreateFrameNodeAutoId(process_node_.get(), page_node_.get());
  MakePageNodeDiscardable(page_node(), task_env());
}

#if !BUILDFLAG(IS_ANDROID)
LenientMockPageDiscarder::LenientMockPageDiscarder() = default;
LenientMockPageDiscarder::~LenientMockPageDiscarder() = default;

std::optional<base::ByteCount> LenientMockPageDiscarder::DiscardPageNode(
    const PageNode* page_node,
    ::mojom::LifecycleUnitDiscardReason discard_reason) {
  if (DiscardPageNodeImpl(page_node)) {
    // Discard success: Return a non-nullopt estimated memory freed.
    return base::ByteCount(0);
  }
  // Discard failure: return nullopt;
  return std::nullopt;
}

GraphTestHarnessWithMockDiscarder::GraphTestHarnessWithMockDiscarder() =
    default;
GraphTestHarnessWithMockDiscarder::~GraphTestHarnessWithMockDiscarder() =
    default;

void GraphTestHarnessWithMockDiscarder::SetUp() {
  GraphTestHarnessWithDiscardablePage::SetUp();

  performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
      local_state_.registry());
  user_performance_tuning_manager_environment_.SetUp(&local_state_);

  // Make the policy use a mock PageDiscarder.
  auto mock_discarder = std::make_unique<MockPageDiscarder>();
  mock_discarder_ = mock_discarder.get();

  // Create the helper and pass it to the graph.
  auto page_discarding_helper =
      std::make_unique<policies::PageDiscardingHelper>();
  page_discarding_helper->SetMockDiscarderForTesting(std::move(mock_discarder));
  graph()->PassToGraph(std::move(page_discarding_helper));
  DCHECK(policies::PageDiscardingHelper::GetFromGraph(graph()));
}

void GraphTestHarnessWithMockDiscarder::TearDown() {
  user_performance_tuning_manager_environment_.TearDown();
  GraphTestHarnessWithDiscardablePage::TearDown();
}
#endif  // !BUILDFLAG(IS_ANDROID)

void ExpectCanDiscardEligible(const PageNode* page_node,
                              std::vector<DiscardReason> discard_reasons,
                              base::TimeDelta minimum_time_in_background) {
  DiscardEligibilityPolicy* policy =
      DiscardEligibilityPolicy::GetFromGraph(page_node->GetGraph());
  for (const DiscardReason discard_reason : discard_reasons) {
    std::vector<CannotDiscardReason> reasons_vec;
    CanDiscardResult result = policy->CanDiscard(
        page_node, discard_reason, minimum_time_in_background, &reasons_vec);
    EXPECT_EQ(CanDiscardResult::kEligible, result);
    EXPECT_TRUE(reasons_vec.empty());
  }
}

void ExpectCanDiscardEligibleAllReasons(
    const PageNode* page_node,
    base::TimeDelta minimum_time_in_background) {
  ExpectCanDiscardEligible(
      page_node,
      {DiscardReason::EXTERNAL, DiscardReason::URGENT, DiscardReason::PROACTIVE,
       DiscardReason::SUGGESTED, DiscardReason::FROZEN_WITH_GROWING_MEMORY},
      minimum_time_in_background);
}

void ExpectCanDiscardProtected(const PageNode* page_node,
                               std::vector<DiscardReason> discard_reasons,
                               CannotDiscardReason protected_reason) {
  DiscardEligibilityPolicy* policy =
      DiscardEligibilityPolicy::GetFromGraph(page_node->GetGraph());
  for (const DiscardReason discard_reason : discard_reasons) {
    std::vector<CannotDiscardReason> reasons_vec;
    CanDiscardResult result = policy->CanDiscard(
        page_node, discard_reason,
        policies::kNonVisiblePagesUrgentProtectionTime, &reasons_vec);
    EXPECT_EQ(CanDiscardResult::kProtected, result);
    EXPECT_TRUE(base::Contains(reasons_vec, protected_reason));
  }
}

void ExpectCanDiscardDisallowedAllReasons(
    const PageNode* page_node,
    CannotDiscardReason disallowed_reason) {
  std::vector<DiscardReason> discard_reasons = {
      DiscardReason::EXTERNAL, DiscardReason::URGENT, DiscardReason::PROACTIVE,
      DiscardReason::SUGGESTED, DiscardReason::FROZEN_WITH_GROWING_MEMORY};
  DiscardEligibilityPolicy* policy =
      DiscardEligibilityPolicy::GetFromGraph(page_node->GetGraph());
  for (const DiscardReason discard_reason : discard_reasons) {
    std::vector<CannotDiscardReason> reasons_vec;
    CanDiscardResult result = policy->CanDiscard(
        page_node, discard_reason,
        policies::kNonVisiblePagesUrgentProtectionTime, &reasons_vec);
    EXPECT_EQ(CanDiscardResult::kDisallowed, result);
    EXPECT_TRUE(base::Contains(reasons_vec, disallowed_reason));
  }
}

}  // namespace performance_manager::testing
