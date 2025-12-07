// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/discard_page_with_crashed_subframe_policy.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/performance_manager/policies/discard_eligibility_policy.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/graph/frame_node_impl.h"
#include "components/performance_manager/graph/page_node_impl.h"
#include "components/performance_manager/graph/process_node_impl.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace performance_manager::policies {

namespace {

struct MockPageGraph {
  TestNodeWrapper<ProcessNodeImpl> process_node;
  TestNodeWrapper<PageNodeImpl> page_node;
  TestNodeWrapper<FrameNodeImpl> main_frame_node;
  TestNodeWrapper<FrameNodeImpl> sub_frame_node;
};

}  // namespace

class DiscardPageWithCrashedSubframePolicyTest
    : public ChromeRenderViewHostTestHarness {
 protected:
  DiscardPageWithCrashedSubframePolicyTest()
      : graph_(std::make_unique<TestGraphImpl>()) {}
  ~DiscardPageWithCrashedSubframePolicyTest() override = default;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    graph_->SetUp();

    // The DiscardPageWithCrashedSubframePolicy depends on the
    // DiscardEligibilityPolicy.
    graph_->PassToGraph(std::make_unique<DiscardEligibilityPolicy>());
    graph_->PassToGraph(
        std::make_unique<DiscardPageWithCrashedSubframePolicy>());

    DiscardEligibilityPolicy::GetFromGraph(graph_.get())
        ->SetNoDiscardPatternsForProfile(GetBrowserContext()->UniqueId(), {});
  }

  void TearDown() override {
    graph_->TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  TestGraphImpl* graph() { return graph_.get(); }

  MockPageGraph CreatePageNodes() {
    auto process_node = TestNodeWrapper<ProcessNodeImpl>::Create(graph_.get());
    auto page_node = TestNodeWrapper<PageNodeImpl>::Create(
        graph_.get(), web_contents()->GetWeakPtr(),
        GetBrowserContext()->UniqueId());
    // Set page node properties to make it eligible for discarding.
    page_node.get()->SetMainFrameRestoredState(
        GURL("http://foo.com"), /* notification_permission_status= */ blink::
            mojom::PermissionStatus::ASK);
    page_node.get()->SetIsVisible(false);
    page_node.get()->SetType(PageType::kTab);
    auto main_frame_node = graph()->CreateFrameNodeAutoId(
        process_node.get(), page_node.get(), nullptr,
        content::BrowsingInstanceId::FromUnsafeValue(1));
    auto sub_frame_node = graph()->CreateFrameNodeAutoId(
        process_node.get(), page_node.get(), main_frame_node.get(),
        content::BrowsingInstanceId::FromUnsafeValue(2));
    return {std::move(process_node), std::move(page_node),
            std::move(main_frame_node), std::move(sub_frame_node)};
  }

  std::unique_ptr<TestGraphImpl> graph_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(DiscardPageWithCrashedSubframePolicyTest, DiscardInvisiblePage) {
  feature_list_.InitAndEnableFeature(::features::kWebContentsDiscard);
  MockPageGraph page_graph = CreatePageNodes();

  // Setup a page that is not visible, and eligible for discarding.
  page_graph.page_node.get()->SetIsVisible(false);
  page_graph.sub_frame_node.get()->SetIsActive(true);
  page_graph.sub_frame_node.get()->SetIsRendered(true);

  EXPECT_FALSE(web_contents()->WasDiscarded());
  page_graph.sub_frame_node.get()->CrossProcessSubframeRenderProcessGone();
  EXPECT_TRUE(web_contents()->WasDiscarded());
}

TEST_F(DiscardPageWithCrashedSubframePolicyTest, DoNotDiscardVisiblePage) {
  feature_list_.InitAndEnableFeature(::features::kWebContentsDiscard);
  MockPageGraph page_graph = CreatePageNodes();

  page_graph.page_node.get()->SetIsVisible(true);
  page_graph.sub_frame_node.get()->SetIsActive(true);
  page_graph.sub_frame_node.get()->SetIsRendered(true);

  EXPECT_FALSE(web_contents()->WasDiscarded());
  page_graph.sub_frame_node.get()->CrossProcessSubframeRenderProcessGone();
  EXPECT_FALSE(web_contents()->WasDiscarded());
}

TEST_F(DiscardPageWithCrashedSubframePolicyTest,
       DoNotDiscardForNotRenderedSubframe) {
  feature_list_.InitAndEnableFeature(::features::kWebContentsDiscard);
  MockPageGraph page_graph = CreatePageNodes();

  page_graph.page_node.get()->SetIsVisible(false);
  page_graph.sub_frame_node.get()->SetIsActive(true);
  page_graph.sub_frame_node.get()->SetIsRendered(false);

  EXPECT_FALSE(web_contents()->WasDiscarded());
  page_graph.sub_frame_node.get()->CrossProcessSubframeRenderProcessGone();
  EXPECT_FALSE(web_contents()->WasDiscarded());
}

TEST_F(DiscardPageWithCrashedSubframePolicyTest,
       DoNotDiscardForNotActiveSubframe) {
  feature_list_.InitAndEnableFeature(::features::kWebContentsDiscard);
  MockPageGraph page_graph = CreatePageNodes();

  page_graph.page_node.get()->SetIsVisible(false);
  page_graph.sub_frame_node.get()->SetIsActive(false);
  page_graph.sub_frame_node.get()->SetIsRendered(true);

  EXPECT_FALSE(web_contents()->WasDiscarded());
  page_graph.sub_frame_node.get()->CrossProcessSubframeRenderProcessGone();
  EXPECT_FALSE(web_contents()->WasDiscarded());
}

TEST_F(DiscardPageWithCrashedSubframePolicyTest,
       DoNotDiscardForNotDiscardablePage) {
  feature_list_.InitAndEnableFeature(::features::kWebContentsDiscard);
  MockPageGraph page_graph = CreatePageNodes();

  page_graph.page_node.get()->SetIsVisible(false);
  page_graph.sub_frame_node.get()->SetIsActive(true);
  page_graph.sub_frame_node.get()->SetIsRendered(true);
  // Make the page audible, which is not discardable.
  page_graph.page_node.get()->SetIsAudible(true);

  EXPECT_FALSE(web_contents()->WasDiscarded());
  page_graph.sub_frame_node.get()->CrossProcessSubframeRenderProcessGone();
  EXPECT_FALSE(web_contents()->WasDiscarded());
}

TEST_F(DiscardPageWithCrashedSubframePolicyTest,
       DoNotDiscardIfFeatureDisabled) {
  feature_list_.InitAndDisableFeature(::features::kWebContentsDiscard);
  MockPageGraph page_graph = CreatePageNodes();

  page_graph.page_node.get()->SetIsVisible(false);
  page_graph.sub_frame_node.get()->SetIsActive(true);
  page_graph.sub_frame_node.get()->SetIsRendered(true);

  EXPECT_FALSE(web_contents()->WasDiscarded());
  page_graph.sub_frame_node.get()->CrossProcessSubframeRenderProcessGone();
  EXPECT_FALSE(web_contents()->WasDiscarded());
}

}  // namespace performance_manager::policies
