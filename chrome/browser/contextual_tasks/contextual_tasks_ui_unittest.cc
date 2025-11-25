// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_context_controller.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/sessions/content/session_tab_helper.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using testing::_;
using testing::Optional;
using testing::Return;
using testing::ReturnRef;

namespace content {
class WebContents;
}  // namespace content

namespace contextual_tasks {

namespace {

constexpr char kAiPageUrl[] = "https://google.com/search?udm=50";

constexpr char kUuid[] = "10000000-0000-0000-0000-000000000000";

class MockTaskInfoDelegate : public TaskInfoDelegate {
 public:
  MockTaskInfoDelegate() = default;
  ~MockTaskInfoDelegate() override = default;
  const std::optional<base::Uuid>& GetTaskId() override { return task_id_; }

  void SetTaskId(std::optional<base::Uuid> id) override { task_id_ = id; }

  const std::optional<std::string>& GetThreadId() override {
    return thread_id_;
  }

  void SetThreadId(std::optional<std::string> id) override { thread_id_ = id; }

  const std::optional<std::string>& GetThreadTitle() override { return title_; }

  void SetThreadTitle(std::optional<std::string> title) override {
    title_ = title;
  }

  bool IsShownInTab() override { return false; }

  BrowserWindowInterface* GetBrowser() override {
    return &mock_browser_window_interface_;
  }

  content::WebContents* GetWebUIWebContents() override { return nullptr; }

 private:
  std::optional<base::Uuid> task_id_;
  std::optional<std::string> thread_id_;
  std::optional<std::string> title_;
  MockBrowserWindowInterface mock_browser_window_interface_;
};

std::unique_ptr<content::MockNavigationHandle> CreateMockNavigationHandle(
    const GURL& url) {
  auto nav_handle = std::make_unique<content::MockNavigationHandle>();
  nav_handle->set_is_in_primary_main_frame(true);
  nav_handle->set_has_committed(true);
  nav_handle->set_url(url);
  return nav_handle;
}

class MockContextualTasksUiService : public ContextualTasksUiService {
 public:
  MockContextualTasksUiService(
      Profile* profile,
      ContextualTasksContextController* context_controller)
      : ContextualTasksUiService(profile, context_controller) {}
  ~MockContextualTasksUiService() override = default;

  MOCK_METHOD(void,
              OnTaskChangedInPanel,
              (BrowserWindowInterface * browser_window_interface,
               content::WebContents* web_contents,
               const base::Uuid& task_id),
              (override));
};

}  // namespace

class ContextualTasksUiTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    context_controller_ = std::make_unique<
        testing::NiceMock<MockContextualTasksContextController>>();
    service_for_nav_ =
        std::make_unique<testing::NiceMock<MockContextualTasksUiService>>(
            profile_.get(), context_controller_.get());

    profile_ = std::make_unique<TestingProfile>();
    embedded_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), content::SiteInstance::Create(profile_.get()));
  }

  void TearDown() override {
    embedded_web_contents_ = nullptr;
    profile_ = nullptr;
    service_for_nav_ = nullptr;
    context_controller_ = nullptr;
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  void SetupMockDelegate(MockTaskInfoDelegate* delegate,
                         const std::optional<base::Uuid>& task_id,
                         const std::optional<std::string>& thread_id,
                         const std::optional<std::string>& title) {
    if (task_id) {
      delegate->SetTaskId(task_id.value());
    }
    if (thread_id) {
      delegate->SetThreadId(thread_id.value());
    }
    if (title) {
      delegate->SetThreadTitle(title.value());
    }
  }

  std::unique_ptr<content::WebContents> embedded_web_contents_;
  std::unique_ptr<TestingProfile> profile_;

  std::unique_ptr<MockContextualTasksUiService> service_for_nav_;
  std::unique_ptr<MockContextualTasksContextController> context_controller_;
};

TEST_F(ContextualTasksUiTest, ContextControllerUpdatedOnUrlChange) {
  MockTaskInfoDelegate delegate;
  std::optional<base::Uuid> task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  std::optional<std::string> thread_id = "5678";
  std::optional<std::string> turn_id = "1234";
  std::optional<std::string> title = "title";

  SetupMockDelegate(&delegate, task_id, thread_id, title);
  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      context_controller_.get(), &delegate);

  GURL updated_url(kAiPageUrl);
  updated_url = net::AppendQueryParameter(updated_url, "mstk", turn_id.value());
  updated_url =
      net::AppendQueryParameter(updated_url, "mtid", thread_id.value());

  EXPECT_CALL(*context_controller_,
              UpdateThreadForTask(task_id.value(), _, thread_id.value(),
                                  Optional(turn_id), Optional(title)))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChangedInPanel(_, _, _)).Times(0);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(updated_url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

TEST_F(ContextualTasksUiTest,
       ContextControllerUpdatedOnUrlChange_ThreadChange) {
  MockTaskInfoDelegate delegate;
  std::optional<base::Uuid> task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  base::Uuid task_id2 =
      base::Uuid::ParseCaseInsensitive("20000000-0000-0000-0000-000000000000");
  std::optional<std::string> thread_id = "5678";
  std::string thread_id2 = "9876";
  std::optional<std::string> turn_id = "1234";
  std::optional<std::string> title = "title";

  SetupMockDelegate(&delegate, task_id, thread_id, title);
  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      context_controller_.get(), &delegate);

  GURL updated_url(kAiPageUrl);
  updated_url = net::AppendQueryParameter(updated_url, "mstk", "abcd");
  updated_url = net::AppendQueryParameter(updated_url, "mtid", thread_id2);

  EXPECT_CALL(*context_controller_,
              UpdateThreadForTask(task_id2, _, thread_id2, _, _))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChangedInPanel(_, _, _)).Times(1);

  ContextualTask task(task_id2);
  ON_CALL(*context_controller_, CreateTaskFromUrl(_))
      .WillByDefault(Return(task));

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(updated_url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

TEST_F(ContextualTasksUiTest,
       ContextControllerNotUpdatedOnUrlChange_NoThreadId) {
  MockTaskInfoDelegate delegate;
  std::optional<base::Uuid> task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  std::optional<std::string> turn_id = "1234";
  std::optional<std::string> title = "title";

  SetupMockDelegate(&delegate, task_id, std::nullopt, title);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      context_controller_.get(), &delegate);

  GURL updated_url(kAiPageUrl);
  updated_url = net::AppendQueryParameter(updated_url, "mstk", turn_id.value());

  // UpdateThreadForTask() is not called due to missing thread id.
  EXPECT_CALL(*context_controller_, UpdateThreadForTask(_, _, _, _, _))
      .Times(0);
  // No task change events should occur.
  EXPECT_CALL(*service_for_nav_, OnTaskChangedInPanel(_, _, _)).Times(0);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(updated_url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

// The task should still updated without a turn ID.
TEST_F(ContextualTasksUiTest, ContextControllerUpdatedOnUrlChange_NoTurnId) {
  MockTaskInfoDelegate delegate;
  std::optional<base::Uuid> task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  std::optional<std::string> thread_id = "5678";
  std::optional<std::string> title = "title";

  SetupMockDelegate(&delegate, task_id, thread_id, title);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      context_controller_.get(), &delegate);

  GURL updated_url(kAiPageUrl);
  updated_url =
      net::AppendQueryParameter(updated_url, "mtid", thread_id.value());

  EXPECT_CALL(*context_controller_,
              UpdateThreadForTask(task_id.value(), _, thread_id.value(), _,
                                  Optional(title)))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChangedInPanel(_, _, _)).Times(0);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(updated_url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

// A task should be created if there's a change in the thread ID and no
// existing task for that ID.
TEST_F(ContextualTasksUiTest, TaskCreated_ThreadIdChanged) {
  MockTaskInfoDelegate delegate;
  base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  std::optional<std::string> thread_id = "5678";
  std::string query = "koalas";

  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      context_controller_.get(), &delegate);

  GURL url(kAiPageUrl);
  url = net::AppendQueryParameter(url, "q", query);

  // Assume the URL has already produced a thread ID for the new query.
  url = net::AppendQueryParameter(url, "mtid", thread_id.value());

  // Ensure a task is created and the info is pushed to the UI.
  ContextualTask task(task_id);
  ON_CALL(*context_controller_, CreateTaskFromUrl(url))
      .WillByDefault(Return(task));
  ON_CALL(*context_controller_, GetTaskFromServerId(_, thread_id.value()))
      .WillByDefault(Return(std::nullopt));

  EXPECT_CALL(*context_controller_, CreateTaskFromUrl(url)).Times(1);
  EXPECT_CALL(*context_controller_, GetTaskFromServerId(_, thread_id.value()))
      .Times(1);
  EXPECT_CALL(
      *context_controller_,
      UpdateThreadForTask(task_id, _, thread_id.value(), _, Optional(query)))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChangedInPanel(_, _, task_id)).Times(1);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

// Ensure a new task isn't created when switching to a thread that already has
// a task.
TEST_F(ContextualTasksUiTest, TaskChanged_ThreadIdChanged_HasExistingTask) {
  MockTaskInfoDelegate delegate;
  base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  std::string thread_id = "5678";
  std::string title = "custom title";

  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      context_controller_.get(), &delegate);

  GURL url(kAiPageUrl);
  url = net::AppendQueryParameter(url, "q", "koalas");
  url = net::AppendQueryParameter(url, "mtid", thread_id);

  // The existing task should be pulled from the service rather than a new one
  // being created.
  ContextualTask task(task_id);
  task.SetTitle(title);
  ON_CALL(*context_controller_, GetTaskFromServerId(_, thread_id))
      .WillByDefault(Return(task));

  EXPECT_CALL(*context_controller_, CreateTaskFromUrl(_)).Times(0);
  EXPECT_CALL(*context_controller_, GetTaskFromServerId(_, thread_id)).Times(1);
  EXPECT_CALL(*context_controller_,
              UpdateThreadForTask(task_id, _, thread_id, _, Optional(title)))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChangedInPanel(_, _, _)).Times(0);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

// The task ID should not change until there's a new thread ID available in the
// URL.
TEST_F(ContextualTasksUiTest, TaskUnchangedWithNoThreadId) {
  MockTaskInfoDelegate delegate;

  base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  std::string thread_id = "5678";
  SetupMockDelegate(&delegate, task_id, thread_id, std::nullopt);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      context_controller_.get(), &delegate);

  GURL url(kAiPageUrl);

  // There is no query value and no other information, the task and thread being
  // tracked should remain unchanged.
  EXPECT_CALL(*context_controller_, CreateTaskFromUrl(_)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnTaskChangedInPanel(_, _, _)).Times(0);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

// Ensure that a pending task (a task without a thread) is not removed and a
// new task created when a thread is finally available.
TEST_F(ContextualTasksUiTest, PendingTaskNoNewTaskCreatedOnNav) {
  MockTaskInfoDelegate delegate;

  base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  SetupMockDelegate(&delegate, task_id, std::nullopt, std::nullopt);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      context_controller_.get(), &delegate);

  GURL url(kAiPageUrl);
  url = net::AppendQueryParameter(url, "mtid", "5678");
  url = net::AppendQueryParameter(url, "mstk", "1234");

  // There is no query value and no other information, the task and thread being
  // tracked should remain unchanged.
  EXPECT_CALL(*context_controller_, CreateTaskFromUrl(_)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnTaskChangedInPanel(_, _, _)).Times(0);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(url);

  observer->DidFinishNavigation(nav_handle.get());

  EXPECT_EQ(delegate.GetTaskId(), task_id);

  observer.reset();
}

}  // namespace contextual_tasks
