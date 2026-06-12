// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_window_tracker.h"

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/test/run_until.h"
#include "base/test/task_environment.h"
#include "base/uuid.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/testing_profile.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/unowned_user_data/scoped_unowned_user_data.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "url/gurl.h"

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

namespace contextual_tasks {

class ContextualTasksWindowTrackerTest
    : public content::RenderViewHostTestHarness {
 public:
  ContextualTasksWindowTrackerTest()
      : content::RenderViewHostTestHarness(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}

 protected:
  std::unique_ptr<content::BrowserContext> CreateBrowserContext() override {
    return std::make_unique<TestingProfile>();
  }

  void SetUp() override { content::RenderViewHostTestHarness::SetUp(); }

  void TearDown() override { content::RenderViewHostTestHarness::TearDown(); }

  base::Uuid task_id_ = base::Uuid::GenerateRandomV4();
  GURL expected_url_ = GURL("https://example.com");
};

TEST_F(ContextualTasksWindowTrackerTest, TimeoutCallsCallback) {
  NiceMock<MockTabListInterface> mock_tab_list;
  bool callback_called = false;
  ContextualTasksWindowTracker* callback_tracker_arg = nullptr;

  auto tracker = std::make_unique<ContextualTasksWindowTracker>(
      &mock_tab_list, task_id_, expected_url_,
      base::BindOnce(
          [](bool* called, ContextualTasksWindowTracker** arg,
             ContextualTasksWindowTracker* t) {
            *called = true;
            *arg = t;
          },
          &callback_called, &callback_tracker_arg));

  EXPECT_FALSE(callback_called);

  // Fast forward time by 10 seconds.
  task_environment()->FastForwardBy(base::Seconds(10));

  EXPECT_TRUE(callback_called);
  EXPECT_EQ(callback_tracker_arg, tracker.get());
}

TEST_F(ContextualTasksWindowTrackerTest, TabRemovedCallsCallback) {
  NiceMock<MockTabListInterface> mock_tab_list;
  bool callback_called = false;

  auto tracker = std::make_unique<ContextualTasksWindowTracker>(
      &mock_tab_list, task_id_, expected_url_,
      base::BindOnce(
          [](bool* called, ContextualTasksWindowTracker*) { *called = true; },
          &callback_called));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      browser_context(), content::SiteInstance::Create(browser_context()));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(expected_url_);

  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents).WillByDefault(Return(web_contents.get()));

  tracker->OnTabAdded(mock_tab_list, &mock_tab, 0);
  EXPECT_FALSE(callback_called);

  tracker->OnTabRemoved(mock_tab_list, &mock_tab, TabRemovedReason::kDeleted);
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_called; }));
}

TEST_F(ContextualTasksWindowTrackerTest, TabListDestroyedCallsCallback) {
  NiceMock<MockTabListInterface> mock_tab_list;
  bool callback_called = false;

  auto tracker = std::make_unique<ContextualTasksWindowTracker>(
      &mock_tab_list, task_id_, expected_url_,
      base::BindOnce(
          [](bool* called, ContextualTasksWindowTracker*) { *called = true; },
          &callback_called));

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      browser_context(), content::SiteInstance::Create(browser_context()));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(expected_url_);

  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents).WillByDefault(Return(web_contents.get()));

  tracker->OnTabAdded(mock_tab_list, &mock_tab, 0);
  EXPECT_FALSE(callback_called);

  tracker->OnTabListDestroyed(mock_tab_list);
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_called; }));
}

TEST_F(ContextualTasksWindowTrackerTest,
       TabListDestroyedBeforeFoundCallsCallback) {
  NiceMock<MockTabListInterface> mock_tab_list;
  bool callback_called = false;

  auto tracker = std::make_unique<ContextualTasksWindowTracker>(
      &mock_tab_list, task_id_, expected_url_,
      base::BindOnce(
          [](bool* called, ContextualTasksWindowTracker*) { *called = true; },
          &callback_called));

  EXPECT_FALSE(callback_called);

  tracker->OnTabListDestroyed(mock_tab_list);
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_called; }));
}

TEST_F(ContextualTasksWindowTrackerTest, BrowserCreatedAddsObserver) {
  NiceMock<MockTabListInterface> mock_tab_list;
  bool callback_called = false;

  auto tracker = std::make_unique<ContextualTasksWindowTracker>(
      nullptr, task_id_, expected_url_,
      base::BindOnce(
          [](bool* called, ContextualTasksWindowTracker*) { *called = true; },
          &callback_called));

  MockBrowserWindowInterface mock_browser;
  ui::UnownedUserDataHost user_data_host;
  EXPECT_CALL(mock_browser, GetUnownedUserDataHost())
      .WillRepeatedly(ReturnRef(user_data_host));
  ui::ScopedUnownedUserData<TabListInterface> scoped_user_data(
      mock_browser.GetUnownedUserDataHost(), mock_tab_list);

  tracker->OnBrowserCreated(&mock_browser);

  auto web_contents = content::WebContentsTester::CreateTestWebContents(
      browser_context(), content::SiteInstance::Create(browser_context()));
  content::WebContentsTester::For(web_contents.get())
      ->NavigateAndCommit(expected_url_);

  tabs::MockTabInterface mock_tab;
  ON_CALL(mock_tab, GetContents).WillByDefault(Return(web_contents.get()));

  tracker->OnTabAdded(mock_tab_list, &mock_tab, 0);

  tracker->OnTabRemoved(mock_tab_list, &mock_tab, TabRemovedReason::kDeleted);
  EXPECT_TRUE(base::test::RunUntil([&]() { return callback_called; }));
}

}  // namespace contextual_tasks
