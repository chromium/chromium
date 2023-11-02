// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/test_support/page_discarding_utils.h"

#include "base/time/time.h"
#include "chrome/browser/performance_manager/decorators/page_aggregator.h"
#include "chrome/browser/performance_manager/decorators/page_live_state_decorator_delegate_impl.h"
#include "chrome/browser/performance_manager/policies/page_discarding_helper.h"
#include "components/performance_manager/decorators/freezing_vote_decorator.h"
#include "components/performance_manager/freezing/freezing_vote_aggregator.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/graph_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/public/decorators/page_live_state_decorator.h"
#include "components/performance_manager/test_support/graph_test_harness.h"

namespace performance_manager {
namespace testing {

LenientMockPageDiscarder::LenientMockPageDiscarder() = default;
LenientMockPageDiscarder::~LenientMockPageDiscarder() = default;

void LenientMockPageDiscarder::DiscardPageNodes(
    const std::vector<const PageNode*>& page_nodes,
    base::OnceCallback<void(bool)> post_discard_cb) {
  bool result = false;
  for (auto* node : page_nodes) {
    if (DiscardPageNodeImpl(node))
      result = true;
  }
  std::move(post_discard_cb).Run(result);
}

GraphTestHarnessWithMockDiscarder::GraphTestHarnessWithMockDiscarder()
    : GraphTestHarness(base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

GraphTestHarnessWithMockDiscarder::~GraphTestHarnessWithMockDiscarder() =
    default;

void GraphTestHarnessWithMockDiscarder::SetUp() {
  GraphTestHarness::SetUp();

  // Some tests depends on the existence of the PageAggregator.
  graph()->PassToGraph(std::make_unique<PageAggregator>());

  // Make the policy use a mock PageDiscarder.
  auto mock_discarder = std::make_unique<MockPageDiscarder>();
  mock_discarder_ = mock_discarder.get();

  // The discarding logic relies on the existence of the page live state data.
  graph()->PassToGraph(std::make_unique<PageLiveStateDecorator>(
      PageLiveStateDelegateImpl::Create()));

  // Create the helper and pass it to the graph.
  auto page_discarding_helper =
      std::make_unique<policies::PageDiscardingHelper>();
  page_discarding_helper->SetMockDiscarderForTesting(std::move(mock_discarder));

  graph()->PassToGraph(std::move(page_discarding_helper));
  DCHECK(policies::PageDiscardingHelper::GetFromGraph(graph()));

  // Create a PageNode and make it discardable.
  process_node_ = CreateNode<performance_manager::ProcessNodeImpl>();
  page_node_ = CreateNode<performance_manager::PageNodeImpl>();
  main_frame_node_ =
      CreateFrameNodeAutoId(process_node_.get(), page_node_.get());
  main_frame_node_->SetIsCurrent(true);
  MakePageNodeDiscardable(page_node(), task_env());
}

void GraphTestHarnessWithMockDiscarder::TearDown() {
  main_frame_node_.reset();
  page_node_.reset();
  process_node_.reset();
  GraphTestHarness::TearDown();
}

void MakePageNodeDiscardable(PageNodeImpl* page_node,
                             content::BrowserTaskEnvironment& task_env) {
  page_node->SetIsVisible(false);
  page_node->SetIsAudible(false);
  const auto kUrl = GURL("https://foo.com");
  page_node->OnMainFrameNavigationCommitted(false, base::TimeTicks::Now(), 42,
                                            kUrl, "text/html");
  (*page_node->main_frame_nodes().begin())->OnNavigationCommitted(kUrl, false);
  task_env.FastForwardBy(base::Minutes(10));
  DCHECK(policies::PageDiscardingHelper::GetFromGraph(page_node->graph())
             ->CanUrgentlyDiscardForTesting(page_node));
}

}  // namespace testing
}  // namespace performance_manager
