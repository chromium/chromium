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

namespace performance_manager::testing {

void MakePageNodeDiscardable(PageNodeImpl* page_node,
                             content::BrowserTaskEnvironment& task_env) {
  using CanDiscardResult = policies::CanDiscardResult;
  using DiscardReason = policies::DiscardEligibilityPolicy::DiscardReason;

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

std::optional<uint64_t> LenientMockPageDiscarder::DiscardPageNode(
    const PageNode* page_node,
    ::mojom::LifecycleUnitDiscardReason discard_reason) {
  if (DiscardPageNodeImpl(page_node)) {
    // Discard success: Return a non-nullopt estimated memory freed.
    return 0;
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

}  // namespace performance_manager::testing
