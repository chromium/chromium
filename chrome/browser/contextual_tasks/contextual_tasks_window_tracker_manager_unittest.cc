// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker_manager.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_types.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/global_routing_id.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Return;

namespace contextual_tasks {

class ContextualTasksWindowTrackerManagerTest
    : public content::RenderViewHostTestHarness {
 protected:
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

  void SetUp() override {
    content::RenderViewHostTestHarness::SetUp();
    manager_ = std::make_unique<ContextualTasksWindowTrackerManager>(
        Profile::FromBrowserContext(browser_context()));
  }

  void TearDown() override {
    manager_.reset();
    content::RenderViewHostTestHarness::TearDown();
  }

  std::unique_ptr<ContextualTasksWindowTrackerManager> manager_;
};

TEST_F(ContextualTasksWindowTrackerManagerTest, AddAndRemoveTracker) {
  auto tracker = std::make_unique<ContextualTasksWindowTracker>(
      ContextualTaskId(base::Uuid::GenerateRandomV4()),
      GURL("https://example.com"), content::GlobalRenderFrameHostToken(),
      nullptr, base::DoNothing());
  auto* tracker_ptr = tracker.get();
  manager_->AddTracker(std::move(tracker));

  EXPECT_EQ(1U, manager_->window_trackers_for_testing().size());

  manager_->RemoveTracker(tracker_ptr);
  EXPECT_TRUE(manager_->window_trackers_for_testing().empty());
}

TEST_F(ContextualTasksWindowTrackerManagerTest,
       MatchAndAssociatePendingTracker_FallbackVectorMatch) {
  GURL url("https://example.com");
  auto tracker = std::make_unique<ContextualTasksWindowTracker>(
      ContextualTaskId(base::Uuid::GenerateRandomV4()), url,
      content::GlobalRenderFrameHostToken(), nullptr, base::DoNothing());
  auto* tracker_ptr = tracker.get();
  // Do not add to pending map, only to vector.
  manager_->AddTracker(std::move(tracker));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      browser_context(), content::SiteInstance::Create(browser_context()));

  ContextualTasksWindowTracker* matched =
      manager_->MatchAndAssociatePendingTracker(url, web_contents.get(),
                                                nullptr);
  EXPECT_EQ(matched, tracker_ptr);
}

TEST_F(ContextualTasksWindowTrackerManagerTest, OnTabAdded_OpenerMatch) {
  auto initiator_contents = content::WebContentsTester::CreateTestWebContents(
      browser_context(), content::SiteInstance::Create(browser_context()));

  GURL url("https://example.com");
  auto tracker = std::make_unique<ContextualTasksWindowTracker>(
      ContextualTaskId(base::Uuid::GenerateRandomV4()), url,
      initiator_contents->GetPrimaryMainFrame()->GetGlobalFrameToken(), nullptr,
      base::DoNothing());
  auto* tracker_ptr = tracker.get();
  manager_->AddTracker(std::move(tracker));

  // Create source_contents with initiator as opener.
  content::WebContents::CreateParams params(browser_context());
  content::GlobalRenderFrameHostId opener_id =
      initiator_contents->GetPrimaryMainFrame()->GetGlobalId();
  params.opener_render_process_id = opener_id.child_id.GetUnsafeValue();
  params.opener_render_frame_id = opener_id.frame_routing_id;
  std::unique_ptr<content::WebContents> source_contents =
      content::WebContents::Create(params);

  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents).WillByDefault(Return(source_contents.get()));

  MockTabListInterface mock_tab_list;

  tabs::TabLookupFromWebContents::CreateForWebContents(source_contents.get(),
                                                       &mock_tab);

  EXPECT_EQ(nullptr, tracker_ptr->GetTabWebContents());

  manager_->OnTabAdded(mock_tab_list, &mock_tab, 0);

  EXPECT_EQ(source_contents.get(), tracker_ptr->GetTabWebContents());
}

TEST_F(ContextualTasksWindowTrackerManagerTest, OnTabAdded_UrlMatchFallback) {
  GURL url("https://example.com");
  auto tracker = std::make_unique<ContextualTasksWindowTracker>(
      ContextualTaskId(base::Uuid::GenerateRandomV4()), url,
      content::GlobalRenderFrameHostToken(), nullptr, base::DoNothing());
  auto* tracker_ptr = tracker.get();
  manager_->AddTracker(std::move(tracker));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      browser_context(), content::SiteInstance::Create(browser_context()));
  content::WebContentsTester::For(web_contents.get())->NavigateAndCommit(url);

  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents).WillByDefault(Return(web_contents.get()));

  MockTabListInterface mock_tab_list;

  tabs::TabLookupFromWebContents::CreateForWebContents(web_contents.get(),
                                                       &mock_tab);

  EXPECT_EQ(nullptr, tracker_ptr->GetTabWebContents());

  manager_->OnTabAdded(mock_tab_list, &mock_tab, 0);

  EXPECT_EQ(web_contents.get(), tracker_ptr->GetTabWebContents());
}

}  // namespace contextual_tasks
