// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"

#include "base/time/time.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/decorators/page_aggregator.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/public/user_tuning/prefs.h"
#include "components/performance_manager/test_support/graph_test_harness.h"

namespace performance_manager {
namespace testing {

LenientMockPageDiscarder::LenientMockPageDiscarder() = default;
LenientMockPageDiscarder::~LenientMockPageDiscarder() = default;

void LenientMockPageDiscarder::DiscardPageNodes(
    const std::vector<const PageNode*>& page_nodes,
    ::mojom::LifecycleUnitDiscardReason discard_reason,
    base::OnceCallback<void(const std::vector<DiscardEvent>&)>
        post_discard_cb) {
  std::vector<DiscardEvent> discard_events;
  for (auto* node : page_nodes) {
    if (DiscardPageNodeImpl(node))
      discard_events.emplace_back(base::TimeTicks::Now(), 0);
  }
  std::move(post_discard_cb).Run(std::move(discard_events));
}

GraphTestHarnessWithMockDiscarder::GraphTestHarnessWithMockDiscarder()
    : GraphTestHarness(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

GraphTestHarnessWithMockDiscarder::~GraphTestHarnessWithMockDiscarder() =
    default;

void GraphTestHarnessWithMockDiscarder::SetUp() {
  // Some tests depends on the existence of the PageAggregator.
  GetGraphFeatures().EnablePageAggregator();

  GraphTestHarness::SetUp();

  performance_manager::user_tuning::prefs::RegisterLocalStatePrefs(
      local_state_.registry());
  user_performance_tuning_manager_environment_.SetUp(&local_state_);

  // Make the policy use a mock PageDiscarder.
  auto mock_discarder = std::make_unique<MockPageDiscarder>();
  mock_discarder_ = mock_discarder.get();

  // The discarding logic relies on the existence of the page live state data.
  graph()->PassToGraph(std::make_unique<PageLiveStateDecorator>());

  // Create the helper and pass it to the graph.
  auto page_discarding_helper =
      std::make_unique<policies::PageDiscardingHelper>();
  page_discarding_helper->SetMockDiscarderForTesting(std::move(mock_discarder));
  // The PageDiscardingHelper usually keeps track of the relevant patterns on
  // profile creation and deletion. Since no profile is involved in this kind of
  // test, add an empty patterns list associated with the "empty string" browser
  // context ID, which is the one used by default for new PageNodes.
  page_discarding_helper->SetNoDiscardPatternsForProfile("", {});

  graph()->PassToGraph(std::move(page_discarding_helper));
  DCHECK(policies::PageDiscardingHelper::GetFromGraph(graph()));

  // Create a PageNode and make it discardable.
  process_node_ = CreateNode<performance_manager::ProcessNodeImpl>();
  page_node_ = CreateNode<performance_manager::PageNodeImpl>();
  main_frame_node_ =
      CreateFrameNodeAutoId(process_node_.get(), page_node_.get());
  MakePageNodeDiscardable(page_node(), task_env());
}

void GraphTestHarnessWithMockDiscarder::TearDown() {
  main_frame_node_.reset();
  page_node_.reset();
  process_node_.reset();
  user_performance_tuning_manager_environment_.TearDown();
  GraphTestHarness::TearDown();
}

void MakePageNodeDiscardable(PageNodeImpl* page_node,
                             content::BrowserTaskEnvironment& task_env) {
  using CanDiscardResult = policies::PageDiscardingHelper::CanDiscardResult;
  using DiscardReason = policies::PageDiscardingHelper::DiscardReason;

  page_node->SetIsVisible(false);
  page_node->SetIsAudible(false);
  const auto kUrl = GURL("https://foo.com");
  page_node->OnMainFrameNavigationCommitted(
      false, base::TimeTicks::Now(), 42, kUrl, "text/html",
      /*notification_permission_status=*/blink::mojom::PermissionStatus::ASK);
  (*page_node->main_frame_nodes().begin())
      ->OnNavigationCommitted(kUrl, url::Origin::Create(kUrl),
                              /*same_document=*/false,
                              /*is_served_from_back_forward_cache=*/false);
  task_env.FastForwardBy(base::Minutes(10));
  const auto* helper =
      policies::PageDiscardingHelper::GetFromGraph(page_node->graph());
  CHECK_EQ(helper->CanDiscard(page_node, DiscardReason::URGENT),
           CanDiscardResult::kEligible);
  CHECK_EQ(helper->CanDiscard(page_node, DiscardReason::PROACTIVE),
           CanDiscardResult::kEligible);
  CHECK_EQ(helper->CanDiscard(page_node, DiscardReason::EXTERNAL),
           CanDiscardResult::kEligible);
  CHECK_EQ(helper->CanDiscard(page_node, DiscardReason::SUGGESTED),
           CanDiscardResult::kEligible);
}

}  // namespace testing
}  // namespace performance_manager
