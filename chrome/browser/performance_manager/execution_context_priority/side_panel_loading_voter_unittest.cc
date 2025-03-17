// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/execution_context_priority/side_panel_loading_voter.h"

#include "base/memory/raw_ptr.h"
#include "components/performance_manager/decorators/frame_visibility_decorator.h"
#include "components/performance_manager/public/execution_context/execution_context.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/voting.h"

namespace performance_manager::execution_context_priority {

namespace {

const execution_context::ExecutionContext* GetExecutionContext(
    const FrameNode* frame_node) {
  return execution_context::ExecutionContext::From(frame_node);
}

}  // namespace

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

class SidePanelLoadingVoterTest : public GraphTestHarness {
 public:
  using Super = GraphTestHarness;

  SidePanelLoadingVoterTest() = default;
  ~SidePanelLoadingVoterTest() override = default;

  SidePanelLoadingVoterTest(const SidePanelLoadingVoterTest&) = delete;
  SidePanelLoadingVoterTest& operator=(const SidePanelLoadingVoterTest&) =
      delete;

  void SetUp() override {
    Super::SetUp();
    graph()->PassToGraph(std::make_unique<FrameVisibilityDecorator>());
    side_panel_loading_voter_.InitializeOnGraph(graph(),
                                                observer_.BuildVotingChannel());
  }

  void TearDown() override {
    side_panel_loading_voter_.TearDownOnGraph(graph());
    Super::TearDown();
  }

  // Exposes the DummyVoteObserver to validate expectations.
  const DummyVoteObserver& observer() const { return observer_; }

  VoterId voter_id() const { return side_panel_loading_voter_.voter_id(); }

  void MarkAsSidePanel(const PageNode* page_node) {
    side_panel_loading_voter_.MarkAsSidePanel(page_node);
  }

 private:
  DummyVoteObserver observer_;
  SidePanelLoadingVoter side_panel_loading_voter_;
};

TEST_F(SidePanelLoadingVoterTest, IncreasePriority) {
  auto process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph()));
  auto page(TestNodeWrapper<PageNodeImpl>::Create(graph()));
  auto frame(graph()->CreateFrameNodeAutoId(process.get(), page.get()));

  MarkAsSidePanel(page.get());

  // Pretend a navigation committed to mock the Side Panel loading.
  page->OnMainFrameNavigationCommitted(
      /*same_document=*/false, base::TimeTicks::Now(),
      /*navigation_id=*/1u, GURL("asd"), "text/html", std::nullopt);

  EXPECT_TRUE(
      observer().HasVote(voter_id(), GetExecutionContext(frame.get()),
                         base::TaskPriority::USER_BLOCKING,
                         SidePanelLoadingVoter::kSidePanelLoadingReason));

  // Making the page visible should invalidate the vote.
  page->SetIsVisible(true);

  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(frame.get())));
}

TEST_F(SidePanelLoadingVoterTest, NotMarked) {
  auto process(TestNodeWrapper<TestProcessNodeImpl>::Create(graph()));
  auto page(TestNodeWrapper<PageNodeImpl>::Create(graph()));
  auto frame(graph()->CreateFrameNodeAutoId(process.get(), page.get()));

  // Pretend a navigation committed to mock the Side Panel loading.
  page->OnMainFrameNavigationCommitted(
      /*same_document=*/false, base::TimeTicks::Now(),
      /*navigation_id=*/1u, GURL("asd"), "text/html", std::nullopt);

  // Because the MarkAsSidePanel() was not called. the main frame's priority was
  // not increased.
  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(frame.get())));

  // Making the page visible.
  page->SetIsVisible(true);

  EXPECT_FALSE(
      observer().HasVote(voter_id(), GetExecutionContext(frame.get())));
}

}  // namespace performance_manager::execution_context_priority
