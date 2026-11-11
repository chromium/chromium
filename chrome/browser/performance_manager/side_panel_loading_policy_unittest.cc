// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/public/side_panel_loading_policy.h"

#include <memory>
#include <utility>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/weak_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/priority/side_panel_loading_voter.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/features.h"
#include "components/performance_manager/public/graph/frame_node.h"
#include "components/performance_manager/public/graph/graph.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "components/performance_manager/test_support/voting.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/render_frame_host_test_support.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager::priority {

namespace {

using DummyVoteObserver = voting::test::DummyVoteObserver<Vote>;

class SidePanelLoadingPolicyTest : public ChromeRenderViewHostTestHarness {
 public:
  SidePanelLoadingPolicyTest() = default;
  ~SidePanelLoadingPolicyTest() override = default;

  SidePanelLoadingPolicyTest(const SidePanelLoadingPolicyTest&) = delete;
  SidePanelLoadingPolicyTest& operator=(const SidePanelLoadingPolicyTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_harness_.GetGraphFeatures().EnableFrameVisibilityDecorator();
    pm_harness_.SetGraphImplCallback(base::BindOnce(
        [](SidePanelLoadingVoter* voter, VotingChannel channel,
           GraphImpl* graph) {
          voter->InitializeOnGraph(graph, std::move(channel));
        },
        &side_panel_loading_voter_, observer_.BuildVotingChannel()));
    pm_harness_.SetUp();
  }

  void TearDown() override {
    DeleteContents();
    side_panel_loading_voter_.TearDownOnGraph(PerformanceManager::GetGraph());
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void InitSidePanelLoadingFeature(bool enabled) {
    side_panel_loading_feature_list_.Reset();
    auto feature_list = std::make_unique<base::FeatureList>();
    feature_list->EnableRuntimeMutability(features::kPMSidePanelLoadingVoter,
                                          base::DoNothing());
    side_panel_loading_feature_list_.InitWithFeatureList(
        std::move(feature_list));
    if (!enabled) {
      side_panel_loading_feature_list_.MutateRuntimeMutableFeature(
          features::kPMSidePanelLoadingVoter, /*enabled=*/false);
    }
  }

  std::unique_ptr<content::WebContents> CreateHiddenSidePanelContents() {
    std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
    content::RenderFrameHostTester::For(contents->GetPrimaryMainFrame())
        ->InitializeRenderFrameIfNeeded();
    contents->WasHidden();
    return contents;
  }

  const FrameNode* GetMainFrameNode(content::WebContents* contents) {
    base::WeakPtr<PageNode> page_node =
        PerformanceManager::GetPrimaryPageNodeForWebContents(contents);
    EXPECT_TRUE(page_node);
    return page_node ? page_node->GetPrimaryMainFrameNode() : nullptr;
  }

  const DummyVoteObserver& observer() const { return observer_; }
  VoterId voter_id() const { return side_panel_loading_voter_.voter_id(); }

 protected:
  base::test::ScopedFeatureList side_panel_loading_feature_list_;

 private:
  PerformanceManagerTestHarnessHelper pm_harness_;
  DummyVoteObserver observer_;
  SidePanelLoadingVoter side_panel_loading_voter_;
};

TEST_F(SidePanelLoadingPolicyTest, FeatureEnabled) {
  InitSidePanelLoadingFeature(/*enabled=*/true);
  auto contents = CreateHiddenSidePanelContents();

  MarkAsSidePanel(contents.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      contents.get(), GURL("https://example.com/"));

  const FrameNode* frame_node = GetMainFrameNode(contents.get());
  ASSERT_TRUE(frame_node);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), frame_node, base::Process::Priority::kUserBlocking,
      SidePanelLoadingVoter::kSidePanelLoadingReason));

  contents->WasShown();
  EXPECT_FALSE(observer().HasVote(voter_id(), frame_node));
}

TEST_F(SidePanelLoadingPolicyTest, FeatureDisabled) {
  InitSidePanelLoadingFeature(/*enabled=*/false);
  auto contents = CreateHiddenSidePanelContents();

  MarkAsSidePanel(contents.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      contents.get(), GURL("https://example.com/"));

  const FrameNode* frame_node = GetMainFrameNode(contents.get());
  ASSERT_TRUE(frame_node);
  EXPECT_FALSE(observer().HasVote(voter_id(), frame_node));
}

TEST_F(SidePanelLoadingPolicyTest, RuntimeMutation) {
  // 1. Starting enabled: MarkAsSidePanel() tags the page and boosts its initial
  // load while hidden.
  InitSidePanelLoadingFeature(/*enabled=*/true);
  auto contents1 = CreateHiddenSidePanelContents();
  MarkAsSidePanel(contents1.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      contents1.get(), GURL("https://example.com/1"));

  const FrameNode* frame1 = GetMainFrameNode(contents1.get());
  ASSERT_TRUE(frame1);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), frame1, base::Process::Priority::kUserBlocking,
      SidePanelLoadingVoter::kSidePanelLoadingReason));

  // 2. Disabling at runtime while `contents1` still holds an active vote:
  side_panel_loading_feature_list_.MutateRuntimeMutableFeature(
      features::kPMSidePanelLoadingVoter, /*enabled=*/false);

  // Showing `contents1` still clears its active vote cleanly.
  contents1->WasShown();
  EXPECT_FALSE(observer().HasVote(voter_id(), frame1));

  // Subsequent `MarkAsSidePanel()` calls while disabled are no-ops.
  auto contents2 = CreateHiddenSidePanelContents();
  MarkAsSidePanel(contents2.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      contents2.get(), GURL("https://example.com/2"));

  const FrameNode* frame2 = GetMainFrameNode(contents2.get());
  ASSERT_TRUE(frame2);
  EXPECT_FALSE(observer().HasVote(voter_id(), frame2));

  // 3. Re-enabling at runtime restores `MarkAsSidePanel()` priority boosting.
  side_panel_loading_feature_list_.MutateRuntimeMutableFeature(
      features::kPMSidePanelLoadingVoter, /*enabled=*/true);

  auto contents3 = CreateHiddenSidePanelContents();
  MarkAsSidePanel(contents3.get());
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      contents3.get(), GURL("https://example.com/3"));

  const FrameNode* frame3 = GetMainFrameNode(contents3.get());
  ASSERT_TRUE(frame3);
  EXPECT_TRUE(observer().HasVote(
      voter_id(), frame3, base::Process::Priority::kUserBlocking,
      SidePanelLoadingVoter::kSidePanelLoadingReason));

  contents3->WasShown();
  EXPECT_FALSE(observer().HasVote(voter_id(), frame3));
}

}  // namespace

}  // namespace performance_manager::priority
