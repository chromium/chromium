// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_manager/policies/process_rank_policy_android.h"

#include "base/memory/raw_ptr.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/flags/android/chrome_feature_list.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/public/graph/page_node.h"
#include "components/performance_manager/test_support/graph_test_harness.h"
#include "content/public/browser/android/child_process_importance.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace performance_manager::policies {

class ProcessRankPolicyAndroidTest : public ChromeRenderViewHostTestHarness {
 public:
  ProcessRankPolicyAndroidTest() : graph_(std::make_unique<TestGraphImpl>()) {}
  ~ProcessRankPolicyAndroidTest() override = default;
  ProcessRankPolicyAndroidTest(const ProcessRankPolicyAndroidTest& other) =
      delete;
  ProcessRankPolicyAndroidTest& operator=(const ProcessRankPolicyAndroidTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    graph_->SetUp();

    graph_->PassToGraph(std::make_unique<ProcessRankPolicyAndroid>());
  }

  void TearDown() override {
    graph_->TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
    scoped_feature_list_.Reset();
  }

  TestGraphImpl* graph() { return graph_.get(); }

  std::unique_ptr<TestGraphImpl> graph_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(ProcessRankPolicyAndroidTest, FocusedPage) {
  TestNodeWrapper<PageNodeImpl> page = TestNodeWrapper<PageNodeImpl>::Create(
      graph(), web_contents()->GetWeakPtr());

  page.get()->SetIsFocused(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::IMPORTANT);
}

TEST_F(ProcessRankPolicyAndroidTest,
       NonFocusedVisiblePageWithChangeUnfocusedPriority) {
  scoped_feature_list_.InitAndEnableFeature(
      chrome::android::kChangeUnfocusedPriority);
  TestNodeWrapper<PageNodeImpl> page = TestNodeWrapper<PageNodeImpl>::Create(
      graph(), web_contents()->GetWeakPtr());

  page.get()->SetIsFocused(false);
  page.get()->SetIsVisible(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::MODERATE);
}

TEST_F(ProcessRankPolicyAndroidTest, NonFocusedVisiblePage) {
  scoped_feature_list_.InitAndDisableFeature(
      chrome::android::kChangeUnfocusedPriority);
  TestNodeWrapper<PageNodeImpl> page = TestNodeWrapper<PageNodeImpl>::Create(
      graph(), web_contents()->GetWeakPtr());

  page.get()->SetIsFocused(false);
  page.get()->SetIsVisible(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::IMPORTANT);
}

TEST_F(ProcessRankPolicyAndroidTest, NonVisibleActivePage) {
  TestNodeWrapper<PageNodeImpl> page = TestNodeWrapper<PageNodeImpl>::Create(
      graph(), web_contents()->GetWeakPtr());

  page.get()->SetIsFocused(false);
  page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page.get())
      ->SetIsActiveTabForTesting(true);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::MODERATE);
}

TEST_F(ProcessRankPolicyAndroidTest, NonVisiblePage) {
  TestNodeWrapper<PageNodeImpl> page = TestNodeWrapper<PageNodeImpl>::Create(
      graph(), web_contents()->GetWeakPtr());

  page.get()->SetIsFocused(false);
  page.get()->SetIsVisible(false);
  PageLiveStateDecorator::Data::GetOrCreateForPageNode(page.get())
      ->SetIsActiveTabForTesting(false);

  EXPECT_EQ(web_contents()->GetPrimaryMainFrameImportanceForTesting(),
            content::ChildProcessImportance::NORMAL);
}

}  // namespace performance_manager::policies
