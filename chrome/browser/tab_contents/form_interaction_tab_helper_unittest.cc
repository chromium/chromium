// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/graph_impl.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/page_aggregator.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"

class FormInteractionTabHelperTest : public ChromeRenderViewHostTestHarness {
 public:
  FormInteractionTabHelperTest() = default;
  ~FormInteractionTabHelperTest() override = default;
  FormInteractionTabHelperTest(const FormInteractionTabHelperTest& other) =
      delete;
  FormInteractionTabHelperTest& operator=(const FormInteractionTabHelperTest&) =
      delete;

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    pm_harness_.SetUp();
    performance_manager::testing::CreatePageAggregatorAndPassItToGraph();
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE, base::BindOnce([](performance_manager::Graph* graph) {
          graph->PassToGraph(FormInteractionTabHelper::CreateGraphObserver());
        }));
  }

  std::unique_ptr<content::WebContents> CreateTestWebContents() {
    std::unique_ptr<content::WebContents> contents =
        ChromeRenderViewHostTestHarness::CreateTestWebContents();
    FormInteractionTabHelper::CreateForWebContents(contents.get());
    // Simulate a navigation event to force the initialization of the main
    // frame.
    content::WebContentsTester::For(contents.get())
        ->NavigateAndCommit(GURL("https://foo.com"));
    task_environment()->RunUntilIdle();
    return contents;
  }

  void TearDown() override {
    pm_harness_.TearDown();
    ChromeRenderViewHostTestHarness::TearDown();
  }

  void SetHadFormInteraction(content::RenderFrameHost* rfh) {
    base::RunLoop run_loop;
    // Use a |QuitWhenIdleClosure| as the task posted to the UI thread by
    // PerformanceManager will have a lower priority (USER_VISIBLE) than the one
    // of a QuitClosure's task runner (USER_BLOCKING).
    auto graph_callback = base::BindLambdaForTesting(
        [quit_loop = run_loop.QuitWhenIdleClosure(),
         node = performance_manager::PerformanceManager::
             GetFrameNodeForRenderFrameHost(rfh)]() {
          auto* frame_node =
              performance_manager::FrameNodeImpl::FromNode(node.get());
          frame_node->SetHadFormInteraction();
          std::move(quit_loop).Run();
        });
    performance_manager::PerformanceManager::CallOnGraph(
        FROM_HERE, std::move(graph_callback));
    run_loop.Run();
  }

 private:
  performance_manager::PerformanceManagerTestHarnessHelper pm_harness_;
};

TEST_F(FormInteractionTabHelperTest, HadFormInteractionSingleFrame) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = FormInteractionTabHelper::FromWebContents(contents.get());

  EXPECT_FALSE(helper->had_form_interaction());
  SetHadFormInteraction(contents->GetPrimaryMainFrame());
  EXPECT_TRUE(helper->had_form_interaction());

  // A navigation event should reset the |had_form_interaction| for this page.
  content::WebContentsTester::For(contents.get())
      ->NavigateAndCommit(GURL("https://bar.com"));
  // Some task are posted to the graph after a navigation event, wait for them
  // to complete.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(helper->had_form_interaction());
}

enum class ChildFrameType {
  kIFrame,
  kFencedFrame,
};

class FormInteractionTabHelperWithChildTest
    : public FormInteractionTabHelperTest,
      public testing::WithParamInterface<ChildFrameType> {
 public:
  FormInteractionTabHelperWithChildTest() {
    std::vector<base::test::FeatureRefAndParams> enabled;
    enabled.push_back(
        {blink::features::kFencedFrames, {{"implementation_type", "mparch"}}});
    scoped_feature_list_.InitWithFeaturesAndParameters(
        enabled, std::vector<base::test::FeatureRef>());
  }
  ~FormInteractionTabHelperWithChildTest() override = default;

  FormInteractionTabHelperWithChildTest(
      const FormInteractionTabHelperWithChildTest&) = delete;
  FormInteractionTabHelperWithChildTest& operator=(
      const FormInteractionTabHelperWithChildTest&) = delete;

  content::RenderFrameHost* AppendChild(content::WebContents* contents) {
    auto* parent_tester =
        content::RenderFrameHostTester::For(contents->GetPrimaryMainFrame());
    switch (GetParam()) {
      case ChildFrameType::kIFrame:
        return parent_tester->AppendChild("child");

      case ChildFrameType::kFencedFrame:
        return parent_tester->AppendFencedFrame();
    }
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         FormInteractionTabHelperWithChildTest,
                         ::testing::Values(ChildFrameType::kIFrame,
                                           ChildFrameType::kFencedFrame));

TEST_P(FormInteractionTabHelperWithChildTest, HadFormInteractionInChildFrame) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = FormInteractionTabHelper::FromWebContents(contents.get());

  EXPECT_FALSE(helper->had_form_interaction());

  content::RenderFrameHost* child =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://foochild.com"), AppendChild(contents.get()));

  SetHadFormInteraction(child);
  EXPECT_TRUE(helper->had_form_interaction());

  // A navigation event should reset the |had_form_interaction| for this page.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://barchild.com"), child);

  // Some task are posted to the graph after a navigation event, wait for them
  // to complete.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(helper->had_form_interaction());
}

TEST_P(FormInteractionTabHelperWithChildTest,
       HadFormInteractionInBothMainAndChild) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = FormInteractionTabHelper::FromWebContents(contents.get());

  EXPECT_FALSE(helper->had_form_interaction());

  SetHadFormInteraction(contents->GetPrimaryMainFrame());
  EXPECT_TRUE(helper->had_form_interaction());

  content::RenderFrameHost* child =
      content::NavigationSimulator::NavigateAndCommitFromDocument(
          GURL("https://foochild.com"), AppendChild(contents.get()));

  SetHadFormInteraction(child);
  EXPECT_TRUE(helper->had_form_interaction());

  // The |had_form_interaction| for this page should be still true even though
  // the navigation happens on the child frame, since the main frame have had an
  // interaction.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://barchild.com"), child);

  // Some task are posted to the graph after a navigation event, wait for them
  // to complete.
  task_environment()->RunUntilIdle();

  EXPECT_TRUE(helper->had_form_interaction());
}
