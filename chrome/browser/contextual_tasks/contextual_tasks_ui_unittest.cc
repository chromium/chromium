// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
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

  void SetThreadTurnId(std::optional<std::string> id) override {
    turn_id_ = id;
  }

  const std::optional<std::string>& GetThreadTurnId() { return turn_id_; }

  const std::optional<std::string>& GetThreadTitle() override { return title_; }

  void SetThreadTitle(std::optional<std::string> title) override {
    title_ = title;
  }

  bool IsShownInTab() override { return is_shown_in_tab_; }

  void SetIsShownInTab(bool is_shown_in_tab) {
    is_shown_in_tab_ = is_shown_in_tab;
  }

  BrowserWindowInterface* GetBrowser() override {
    return &mock_browser_window_interface_;
  }

  void SetIsAiPage(bool is_ai_page) override {}

  content::WebContents* GetWebUIWebContents() override { return nullptr; }

  MOCK_METHOD(void, OnZeroStateChange, (bool is_zero_state), (override));

 private:
  std::optional<base::Uuid> task_id_;
  std::optional<std::string> thread_id_;
  std::optional<std::string> turn_id_;
  std::optional<std::string> title_;
  bool is_shown_in_tab_ = false;
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
      contextual_tasks::ContextualTasksService* contextual_tasks_service)
      : ContextualTasksUiService(profile, contextual_tasks_service, nullptr) {}
  ~MockContextualTasksUiService() override = default;

  MOCK_METHOD(void,
              OnTaskChanged,
              (BrowserWindowInterface * browser_window_interface,
               content::WebContents* web_contents,
               const base::Uuid& task_id,
               bool is_shown_in_tab),
              (override));
  MOCK_METHOD(GURL, GetDefaultAiPageUrl, (), (override));
};

}  // namespace

class ContextualTasksUiTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    contextual_tasks_service_ =
        std::make_unique<testing::NiceMock<MockContextualTasksService>>();
    service_for_nav_ =
        std::make_unique<testing::NiceMock<MockContextualTasksUiService>>(
            profile_.get(), contextual_tasks_service_.get());

    profile_ = std::make_unique<TestingProfile>();
    embedded_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_.get(), content::SiteInstance::Create(profile_.get()));
  }

  void TearDown() override {
    embedded_web_contents_ = nullptr;
    profile_ = nullptr;
    service_for_nav_ = nullptr;
    contextual_tasks_service_ = nullptr;
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
  std::unique_ptr<MockContextualTasksService> contextual_tasks_service_;
};

TEST_F(ContextualTasksUiTest, ContextualTasksServiceUpdatedOnUrlChange) {
  MockTaskInfoDelegate delegate;
  std::optional<base::Uuid> task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  std::optional<std::string> thread_id = "5678";
  std::optional<std::string> turn_id = "1234";
  std::optional<std::string> title = "title";

  SetupMockDelegate(&delegate, task_id, thread_id, title);
  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL updated_url(kAiPageUrl);
  updated_url = net::AppendQueryParameter(updated_url, "q", "test");
  updated_url = net::AppendQueryParameter(updated_url, "mstk", turn_id.value());
  updated_url =
      net::AppendQueryParameter(updated_url, "mtid", thread_id.value());

  EXPECT_CALL(*contextual_tasks_service_,
              UpdateThreadForTask(task_id.value(), _, thread_id.value(),
                                  Optional(turn_id), Optional(title)))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, _)).Times(0);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(updated_url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

TEST_F(ContextualTasksUiTest,
       ContextualTasksServiceUpdatedOnUrlChange_ThreadChange) {
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
      contextual_tasks_service_.get(), &delegate);

  GURL updated_url(kAiPageUrl);
  updated_url = net::AppendQueryParameter(updated_url, "q", "test");
  updated_url = net::AppendQueryParameter(updated_url, "mstk", "abcd");
  updated_url = net::AppendQueryParameter(updated_url, "mtid", thread_id2);

  EXPECT_CALL(*contextual_tasks_service_,
              UpdateThreadForTask(task_id2, _, thread_id2, _, _))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, false)).Times(1);

  ContextualTask task(task_id2);
  ON_CALL(*contextual_tasks_service_, CreateTaskFromUrl(_))
      .WillByDefault(Return(task));

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(updated_url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

TEST_F(ContextualTasksUiTest,
       ContextualTasksServiceNotUpdatedOnUrlChange_NoThreadId) {
  MockTaskInfoDelegate delegate;
  std::optional<base::Uuid> task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  std::optional<std::string> turn_id = "1234";
  std::optional<std::string> title = "title";

  SetupMockDelegate(&delegate, task_id, std::nullopt, title);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL updated_url(kAiPageUrl);
  updated_url = net::AppendQueryParameter(updated_url, "q", "test");
  updated_url = net::AppendQueryParameter(updated_url, "mstk", turn_id.value());

  // UpdateThreadForTask() is not called due to missing thread id.
  EXPECT_CALL(*contextual_tasks_service_, UpdateThreadForTask(_, _, _, _, _))
      .Times(0);
  // No task change events should occur.
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, _)).Times(0);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(updated_url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

// The task should still updated without a turn ID.
TEST_F(ContextualTasksUiTest,
       ContextualTasksServiceUpdatedOnUrlChange_NoTurnId) {
  MockTaskInfoDelegate delegate;
  std::optional<base::Uuid> task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  std::optional<std::string> thread_id = "5678";
  std::optional<std::string> title = "title";

  SetupMockDelegate(&delegate, task_id, thread_id, title);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL updated_url(kAiPageUrl);
  updated_url = net::AppendQueryParameter(updated_url, "q", "test");
  updated_url =
      net::AppendQueryParameter(updated_url, "mtid", thread_id.value());

  EXPECT_CALL(*contextual_tasks_service_,
              UpdateThreadForTask(task_id.value(), _, thread_id.value(), _,
                                  Optional(title)))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, _)).Times(0);

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
      contextual_tasks_service_.get(), &delegate);

  GURL url(kAiPageUrl);
  url = net::AppendQueryParameter(url, "q", query);

  // Assume the URL has already produced a thread ID for the new query.
  url = net::AppendQueryParameter(url, "mtid", thread_id.value());

  // Ensure a task is created and the info is pushed to the UI.
  ContextualTask task(task_id);
  ON_CALL(*contextual_tasks_service_, CreateTaskFromUrl(url))
      .WillByDefault(Return(task));
  ON_CALL(*contextual_tasks_service_, GetTaskFromServerId(_, thread_id.value()))
      .WillByDefault(Return(std::nullopt));

  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(url)).Times(1);
  EXPECT_CALL(*contextual_tasks_service_,
              GetTaskFromServerId(_, thread_id.value()))
      .Times(1);
  EXPECT_CALL(
      *contextual_tasks_service_,
      UpdateThreadForTask(task_id, _, thread_id.value(), _, Optional(query)))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, task_id, false)).Times(1);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

// Ensure that OnTaskChanged is called with is_shown_in_tab = true when the
// delegate indicates it is shown in a tab.
TEST_F(ContextualTasksUiTest, TaskCreated_ThreadIdChanged_ShownInTab) {
  MockTaskInfoDelegate delegate;
  delegate.SetIsShownInTab(true);
  base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  std::optional<std::string> thread_id = "5678";
  std::string query = "koalas";

  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL url(kAiPageUrl);
  url = net::AppendQueryParameter(url, "q", query);
  url = net::AppendQueryParameter(url, "mtid", thread_id.value());

  ContextualTask task(task_id);
  ON_CALL(*contextual_tasks_service_, CreateTaskFromUrl(url))
      .WillByDefault(Return(task));
  ON_CALL(*contextual_tasks_service_, GetTaskFromServerId(_, thread_id.value()))
      .WillByDefault(Return(std::nullopt));

  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(url)).Times(1);
  EXPECT_CALL(*contextual_tasks_service_,
              GetTaskFromServerId(_, thread_id.value()))
      .Times(1);
  EXPECT_CALL(
      *contextual_tasks_service_,
      UpdateThreadForTask(task_id, _, thread_id.value(), _, Optional(query)))
      .Times(1);
  // Verify is_shown_in_tab is true.
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, task_id, true)).Times(1);

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
      contextual_tasks_service_.get(), &delegate);

  GURL url(kAiPageUrl);
  url = net::AppendQueryParameter(url, "q", "koalas");
  url = net::AppendQueryParameter(url, "mtid", thread_id);

  // The existing task should be pulled from the service rather than a new one
  // being created.
  ContextualTask task(task_id);
  task.SetTitle(title);
  ON_CALL(*contextual_tasks_service_, GetTaskFromServerId(_, thread_id))
      .WillByDefault(Return(task));

  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(_)).Times(0);
  EXPECT_CALL(*contextual_tasks_service_, GetTaskFromServerId(_, thread_id))
      .Times(1);
  EXPECT_CALL(*contextual_tasks_service_,
              UpdateThreadForTask(task_id, _, thread_id, _, Optional(title)))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, _)).Times(0);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(url);

  observer->DidFinishNavigation(nav_handle.get());

  observer.reset();
}

// A new task should be created when navigating to the zero state.
TEST_F(ContextualTasksUiTest, TaskCreated_ZeroState) {
  MockTaskInfoDelegate delegate;

  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL url(kAiPageUrl);

  base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  ContextualTask task(task_id);
  EXPECT_CALL(*contextual_tasks_service_, CreateTask()).WillOnce(Return(task));
  // OnTaskChanged should be called with an empty UUID for zero state.
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, base::Uuid(), _)).Times(1);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(url);

  observer->DidFinishNavigation(nav_handle.get());

  EXPECT_EQ(delegate.GetTaskId(), task_id);

  observer.reset();
}

TEST_F(ContextualTasksUiTest, ThreadUpdatedOnSameDocumentNav) {
  MockTaskInfoDelegate delegate;
  base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  std::optional<std::string> thread_id = "5678";
  std::string query = "koalas";

  SetupMockDelegate(&delegate, task_id, "1234", std::nullopt);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL url(kAiPageUrl);
  url = net::AppendQueryParameter(url, "q", query);
  url = net::AppendQueryParameter(url, "mtid", thread_id.value());

  EXPECT_CALL(
      *contextual_tasks_service_,
      UpdateThreadForTask(task_id, _, thread_id.value(), _, Optional(query)))
      .Times(1);

  ContextualTask task(task_id);
  ON_CALL(*contextual_tasks_service_, CreateTaskFromUrl(url))
      .WillByDefault(Return(task));

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(url);
  nav_handle->set_is_same_document(true);

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
      contextual_tasks_service_.get(), &delegate);

  GURL url(kAiPageUrl);
  url = net::AppendQueryParameter(url, "q", "test");
  url = net::AppendQueryParameter(url, "mtid", "5678");
  url = net::AppendQueryParameter(url, "mstk", "1234");

  // There is no query value and no other information, the task and thread being
  // tracked should remain unchanged.
  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(_)).Times(0);
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, _)).Times(0);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(url);

  observer->DidFinishNavigation(nav_handle.get());

  EXPECT_EQ(delegate.GetTaskId(), task_id);

  observer.reset();
}

TEST_F(ContextualTasksUiTest, TaskDetailsUpdated) {
  MockTaskInfoDelegate delegate;

  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL url(kAiPageUrl);
  const std::string thread_id = "5678";
  url = net::AppendQueryParameter(url, "q", "test");
  url = net::AppendQueryParameter(url, "mtid", thread_id);
  const std::string turn_id = "1234";
  url = net::AppendQueryParameter(url, "mstk", turn_id);

  // Expect a task to be created
  base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  ContextualTask task(task_id);
  ON_CALL(*contextual_tasks_service_, CreateTaskFromUrl(url))
      .WillByDefault(Return(task));

  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(url)).Times(1);

  std::unique_ptr<content::MockNavigationHandle> nav_handle =
      CreateMockNavigationHandle(url);

  observer->DidFinishNavigation(nav_handle.get());

  EXPECT_EQ(delegate.GetTaskId(), task_id);
  EXPECT_EQ(delegate.GetThreadId(), thread_id);
  EXPECT_EQ(delegate.GetThreadTurnId(), turn_id);

  // Fake an updated turn
  GURL url2(kAiPageUrl);
  url2 = net::AppendQueryParameter(url2, "q", "test");
  url2 = net::AppendQueryParameter(url2, "mtid", thread_id);
  const std::string turn_id2 = "2222";
  url2 = net::AppendQueryParameter(url2, "mstk", turn_id2);

  std::unique_ptr<content::MockNavigationHandle> nav_handle2 =
      CreateMockNavigationHandle(url2);

  observer->DidFinishNavigation(nav_handle2.get());

  EXPECT_EQ(delegate.GetTaskId(), task_id);
  EXPECT_EQ(delegate.GetThreadId(), thread_id);
  EXPECT_EQ(delegate.GetThreadTurnId(), turn_id2);
  observer.reset();
}

TEST_F(ContextualTasksUiTest, DidStartNavigation_ZeroState) {
  struct TestCase {
    GURL url;
    bool expected_is_zero_state;
  } test_cases[] = {
      {GURL("https://google.com"), false},
      {GURL("https://google.com?q=test"), false},
      {GURL("https://www.google.com/search?udm=50"), true},
      {GURL("https://www.google.com/search?udm=50&mstk=test"), false},
      {GURL("https://www.google.com/search?udm=50&q="), true},
      {GURL("https://www.google.com/search?udm=50&q=&mstk=test"), false},
      {GURL("https://www.google.com/search?udm=50&q=&mstk="), true},
      {GURL("https://www.google.com/search?udm=50&q=test"), false},
      {GURL("https://www.google.com/search?udm=50&q=test&mstk="), false},
      {GURL("https://www.google.com/search?udm=50&q=&mstk=&vsrid=test"), false},
      {GURL("https://www.google.com/search?udm=50&q=&mstk=&cinpts=test"),
       false},
      {GURL("https://google.com/search"), false},
  };

  for (const auto& test_case : test_cases) {
    testing::NiceMock<MockTaskInfoDelegate> delegate;
    SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);

    auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
        embedded_web_contents_.get(), service_for_nav_.get(),
        contextual_tasks_service_.get(), &delegate);

    EXPECT_EQ(
        ContextualTasksUI::IsZeroState(test_case.url, service_for_nav_.get()),
        test_case.expected_is_zero_state)
        << "Expected " << test_case.url.spec() << " to "
        << (test_case.expected_is_zero_state ? "be" : "not be")
        << " a zero state";
    EXPECT_CALL(delegate, OnZeroStateChange(test_case.expected_is_zero_state))
        .Times(1);

    if (test_case.expected_is_zero_state) {
      base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
      ContextualTask task(task_id);
      EXPECT_CALL(*contextual_tasks_service_, CreateTask())
          .WillOnce(Return(task));
    }

    std::unique_ptr<content::MockNavigationHandle> nav_handle =
        CreateMockNavigationHandle(test_case.url);

    observer->DidFinishNavigation(nav_handle.get());
  }
}

}  // namespace contextual_tasks
