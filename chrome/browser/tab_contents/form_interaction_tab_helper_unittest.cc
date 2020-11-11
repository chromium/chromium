// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/form_interaction_tab_helper.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/task/task_traits.h"
#include "base/test/bind.h"
#include "chrome/browser/performance_manager/test_support/page_aggregator.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "components/performance_manager/embedder/performance_manager_registry.h"
#include "components/performance_manager/performance_manager_impl.h"
#include "components/performance_manager/public/performance_manager.h"
#include "components/performance_manager/test_support/graph_impl.h"
#include "components/performance_manager/test_support/mock_graphs.h"
#include "components/performance_manager/test_support/test_harness_helper.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    performance_manager::PerformanceManagerImpl::CallOnGraph(
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

 private:
  performance_manager::PerformanceManagerTestHarnessHelper pm_harness_;
};

TEST_F(FormInteractionTabHelperTest, HadFormInteractionSingleFrame) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = FormInteractionTabHelper::FromWebContents(contents.get());

  EXPECT_FALSE(helper->had_form_interaction());

  // Indicates that a form on the main frame has been interacted with.
  {
    base::RunLoop run_loop;
    // Use a |QuitWhenIdleClosure| as the task posted to the UI thread by
    // PerformanceManager will have a lower priority (USER_VISIBLE) than the one
    // of a QuitClosure's task runner (USER_BLOCKING).
    auto graph_callback = base::BindLambdaForTesting(
        [quit_loop = run_loop.QuitWhenIdleClosure(),
         page_node =
             performance_manager::PerformanceManager::GetPageNodeForWebContents(
                 contents.get())]() {
          auto* frame_node = performance_manager::FrameNodeImpl::FromNode(
              page_node->GetMainFrameNode());
          frame_node->SetIsCurrent(true);
          frame_node->SetHadFormInteraction();
          std::move(quit_loop).Run();
        });
    performance_manager::PerformanceManagerImpl::CallOnGraph(
        FROM_HERE, std::move(graph_callback));
    run_loop.Run();
  }

  EXPECT_TRUE(helper->had_form_interaction());

  // A navigation event should reset the |had_form_interaction| for this page.
  content::WebContentsTester::For(contents.get())
      ->NavigateAndCommit(GURL("https://bar.com"));
  // Some task are posted to the graph after a navigation event, wait for them
  // to complete.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(helper->had_form_interaction());
}

TEST_F(FormInteractionTabHelperTest, HadFormInteractionWithChildFrames) {
  std::unique_ptr<content::WebContents> contents = CreateTestWebContents();
  auto* helper = FormInteractionTabHelper::FromWebContents(contents.get());

  EXPECT_FALSE(helper->had_form_interaction());

  auto* parent_tester =
      content::RenderFrameHostTester::For(contents->GetMainFrame());
  auto* child = content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://foochild.com"), parent_tester->AppendChild("child"));

  // Indicates that a form on the child frame has been interacted with.
  {
    base::RunLoop run_loop;
    // Use a |QuitWhenIdleClosure| as the task posted to the UI thread by
    // PerformanceManager will have a lower priority (USER_VISIBLE) than the one
    // of a QuitClosure's task runner (USER_BLOCKING).
    auto graph_callback = base::BindLambdaForTesting(
        [quit_loop = run_loop.QuitWhenIdleClosure(),
         page_node =
             performance_manager::PerformanceManager::GetPageNodeForWebContents(
                 contents.get())]() {
          auto children = page_node->GetMainFrameNode()->GetChildFrameNodes();
          EXPECT_EQ(1U, children.size());
          auto* frame_node =
              performance_manager::FrameNodeImpl::FromNode(*children.begin());
          frame_node->SetIsCurrent(true);
          frame_node->SetHadFormInteraction();
          std::move(quit_loop).Run();
        });
    performance_manager::PerformanceManagerImpl::CallOnGraph(
        FROM_HERE, std::move(graph_callback));
    run_loop.Run();
  }

  EXPECT_TRUE(helper->had_form_interaction());

  // A navigation event should reset the |had_form_interaction| for this page.
  content::NavigationSimulator::NavigateAndCommitFromDocument(
      GURL("https://barchild.com"), child);

  // Some task are posted to the graph after a navigation event, wait for them
  // to complete.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(helper->had_form_interaction());
}
