// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_page.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/ui/browser_window/test/mock_browser_window_interface.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/common/content_features.h"
#include "content/public/test/mock_navigation_handle.h"
#include "content/public/test/test_renderer_host.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/web_preferences/web_preferences.h"
#include "third_party/blink/public/mojom/css/preferred_color_scheme.mojom.h"
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

constexpr char kTestingProfileName[] = "testing_profile";

class MockContextualTasksComposeboxHandler
    : public ContextualTasksComposeboxHandlerInterface {
 public:
  MOCK_METHOD(void, ResetInputStateModel, (), (override));
  MOCK_METHOD(void,
              UpdateSuggestedTabContext,
              (const SuggestedTabInfo*),
              (override));
  MOCK_METHOD(void, OnTaskChanged, (), (override));
  MOCK_METHOD(void, InitializeInputStateModel, (), (override));
  MOCK_METHOD(void, UpdateModelFromUrl, (const GURL&), (override));
};

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

  void SetAimUrl(const GURL& url) override { url_ = url; }
  MOCK_METHOD(void, UpdateModelModeFromUrl, (const GURL& url), (override));

  bool IsShownInTab() override { return is_shown_in_tab_; }

  void SetIsShownInTab(bool is_shown_in_tab) {
    is_shown_in_tab_ = is_shown_in_tab;
  }

  BrowserWindowInterface* GetBrowser() override {
    return &mock_browser_window_interface_;
  }

  void SetIsAiPage(bool is_ai_page) override {}
  void SetInNlm(bool in_nlm) override {}

  content::WebContents* GetWebUIWebContents() override { return nullptr; }

  MOCK_METHOD(void, OnZeroStateChange, (bool is_zero_state), (override));

  MOCK_METHOD(void, PrepareForTaskChange, (), (override));

  MOCK_METHOD(void, OnTaskChanged, (), (override));

 private:
  std::optional<base::Uuid> task_id_;
  std::optional<std::string> thread_id_;
  std::optional<std::string> turn_id_;
  std::optional<std::string> title_;
  GURL url_;
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

}  // namespace

class ContextualTasksUiTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    testing_profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(testing_profile_manager_->SetUp());

    profile_ =
        testing_profile_manager_->CreateTestingProfile(kTestingProfileName);

    auto contextual_tasks_service = std::make_unique<
        testing::NiceMock<contextual_tasks::MockContextualTasksService>>();
    contextual_tasks_service_ = contextual_tasks_service.get();
    ContextualTasksServiceFactory::GetInstance()->SetTestingFactory(
        profile_, base::BindLambdaForTesting(
                      [service = std::move(contextual_tasks_service)](
                          content::BrowserContext* context) mutable
                          -> std::unique_ptr<KeyedService> {
                        return std::move(service);
                      }));

    auto service_for_nav = std::make_unique<
        testing::NiceMock<contextual_tasks::MockContextualTasksUiService>>(
        profile_, contextual_tasks_service_);
    service_for_nav_ = service_for_nav.get();
    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        profile_,
        base::BindLambdaForTesting([service = std::move(service_for_nav)](
                                       content::BrowserContext* context) mutable
                                       -> std::unique_ptr<KeyedService> {
          return std::move(service);
        }));

    ON_CALL(*service_for_nav_, IsAiUrl(_)).WillByDefault(Return(true));

    embedded_web_contents_ = content::WebContentsTester::CreateTestWebContents(
        profile_, content::SiteInstance::Create(profile_));
  }

  void TearDown() override {
    embedded_web_contents_ = nullptr;
    service_for_nav_ = nullptr;
    contextual_tasks_service_ = nullptr;
    if (profile_) {
      ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
          profile_, base::NullCallback());
      ContextualTasksServiceFactory::GetInstance()->SetTestingFactory(
          profile_, base::NullCallback());
    }
    profile_ = nullptr;
    testing_profile_manager_->DeleteTestingProfile(kTestingProfileName);
    testing_profile_manager_.reset();
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
  raw_ptr<TestingProfile> profile_;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;

  raw_ptr<contextual_tasks::MockContextualTasksUiService> service_for_nav_;
  raw_ptr<contextual_tasks::MockContextualTasksService>
      contextual_tasks_service_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
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

  EXPECT_CALL(
      *contextual_tasks_service_,
      UpdateThreadForTask(task_id.value(), _, thread_id.value(),
                          Optional(turn_id), Optional(std::string("test"))))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, _, _)).Times(0);
  EXPECT_CALL(delegate, UpdateModelModeFromUrl(updated_url)).Times(1);

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
  EXPECT_CALL(delegate, PrepareForTaskChange()).Times(1);
  EXPECT_CALL(*service_for_nav_,
              OnTaskChanged(_, _, _, _, /*is_shown_in_tab=*/false))
      .Times(1);

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
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, _, _)).Times(0);

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
                                  Optional(std::string("test"))))
      .Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, _, _)).Times(0);

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
  EXPECT_CALL(delegate, PrepareForTaskChange()).Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, Optional(task_id),
                                               /*is_shown_in_tab=*/false))
      .Times(1);

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
  EXPECT_CALL(delegate, PrepareForTaskChange()).Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, Optional(task_id),
                                               /*is_shown_in_tab=*/true))
      .Times(1);

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
              UpdateThreadForTask(task_id, _, thread_id, _,
                                  Optional(std::string("koalas"))))
      .Times(1);
  EXPECT_CALL(delegate, PrepareForTaskChange()).Times(1);
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, _, _)).Times(1);

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
  // OnTaskChanged should be called with the created UUID.
  EXPECT_CALL(delegate, PrepareForTaskChange()).Times(1);
  EXPECT_CALL(*contextual_tasks_service_, CreateTask()).WillOnce(Return(task));
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, Optional(task_id), _))
      .Times(1);

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

  EXPECT_CALL(delegate, UpdateModelModeFromUrl(url)).Times(1);

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
  EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, _, _)).Times(0);

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

TEST_F(ContextualTasksUiTest, AreUrlsEqual) {
  EXPECT_TRUE(ContextualTasksUI::AreUrlsEqual(
      GURL("https://google.com/search?q=test&udm=50"),
      GURL("https://google.com/search?udm=50&q=test")));

  EXPECT_TRUE(ContextualTasksUI::AreUrlsEqual(
      GURL("https://google.com/search?a=1&b=2&c=3"),
      GURL("https://google.com/search?c=3&a=1&b=2")));

  EXPECT_TRUE(ContextualTasksUI::AreUrlsEqual(
      GURL("https://google.com/search"), GURL("https://google.com/search")));

  // Different query keys/values
  EXPECT_FALSE(ContextualTasksUI::AreUrlsEqual(
      GURL("https://google.com/search?q=test&udm=50"),
      GURL("https://google.com/search?udm=50&q=test2")));

  EXPECT_FALSE(ContextualTasksUI::AreUrlsEqual(
      GURL("https://google.com/search?q=test&udm=50"),
      GURL("https://google.com/search?udm=50&q2=test")));

  // Different paths
  EXPECT_FALSE(ContextualTasksUI::AreUrlsEqual(
      GURL("https://google.com/search?q=test&udm=50"),
      GURL("https://google.com/search2?udm=50&q=test")));

  // Different query param sizes
  EXPECT_FALSE(ContextualTasksUI::AreUrlsEqual(
      GURL("https://google.com/search?q=test&udm=50"),
      GURL("https://google.com/search?udm=50&q=test&extra=1")));
}

TEST_F(ContextualTasksUiTest, DidFinishNavigation_ZeroState) {
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
      {GURL("https://www.google.com/search?q=test&udm=50"), false},
      {GURL("https://www.google.com/search?udm=50&other=param"),
       true},  // Other noise/params
      {GURL("https://www.google.com/search?udm=50&q=%20"),
       false},  // Whitespace
      {GURL("https://www.google.com/search?udm=50&smstk=test"),
       false},  // smstk present
      {GURL("https://www.google.com/search?udm=50&smstk="),
       true},  // smstk empty
  };

  ON_CALL(*service_for_nav_, IsAiUrl(GURL("https://google.com")))
      .WillByDefault(Return(false));
  ON_CALL(*service_for_nav_, IsAiUrl(GURL("https://google.com?q=test")))
      .WillByDefault(Return(false));
  ON_CALL(*service_for_nav_, IsAiUrl(GURL("https://google.com/search")))
      .WillByDefault(Return(false));

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
      EXPECT_CALL(delegate, PrepareForTaskChange()).Times(1);
    }

    std::unique_ptr<content::MockNavigationHandle> nav_handle =
        CreateMockNavigationHandle(test_case.url);

    observer->DidFinishNavigation(nav_handle.get());
  }
}

// Checks that does not create new task when fully refreshing page.
TEST_F(ContextualTasksUiTest, DidFinishNavigation_FiresOnReload) {
  testing::NiceMock<MockTaskInfoDelegate> delegate;
  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);

  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL zero_state_url("https://www.google.com/search?udm=50");

  base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  ContextualTask task(task_id);

  EXPECT_CALL(delegate, OnZeroStateChange(true)).Times(2);
  EXPECT_CALL(delegate, PrepareForTaskChange()).Times(1);
  EXPECT_CALL(*contextual_tasks_service_, CreateTask())
      .Times(1)
      .WillRepeatedly(Return(task));

  EXPECT_CALL(delegate, UpdateModelModeFromUrl(zero_state_url)).Times(2);

  // First load.
  auto handle1 = CreateMockNavigationHandle(zero_state_url);
  handle1->set_has_committed(true);
  observer->DidFinishNavigation(handle1.get());

  // Full refresh, with same URL.
  auto handle2 = CreateMockNavigationHandle(zero_state_url);
  handle2->set_has_committed(true);
  handle2->set_reload_type(content::ReloadType::NORMAL);
  observer->DidFinishNavigation(handle2.get());
}

/* Ensures didFinishNavigation ignores network errors and returns early
 * when !hasCommitted.
 */
TEST_F(ContextualTasksUiTest, DidFinishNavigation_IgnoredCases) {
  testing::NiceMock<MockTaskInfoDelegate> delegate;
  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);
  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  EXPECT_CALL(delegate, OnZeroStateChange(_)).Times(0);

  auto failed_handle = std::make_unique<content::MockNavigationHandle>();
  failed_handle->set_url(GURL("https://www.google.com/search?udm=50"));

  failed_handle->set_is_in_primary_main_frame(true);

  // Returns when !hasCommitted.
  failed_handle->set_has_committed(false);
  observer->DidFinishNavigation(failed_handle.get());
}

/* Goes from zero state to regular state, then refresh, and
 * then back to zero, then regular.
 */
TEST_F(ContextualTasksUiTest, Transition_QueryToZeroToQuery) {
  testing::NiceMock<MockTaskInfoDelegate> delegate;
  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);
  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL zero_state_url("https://www.google.com/search?udm=50");
  GURL query_url("https://www.google.com/search?udm=50&q=cats");

  // Mock functions
  base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  ContextualTask task(task_id);
  ON_CALL(*contextual_tasks_service_, CreateTask()).WillByDefault(Return(task));
  ON_CALL(*contextual_tasks_service_, CreateTaskFromUrl(_))
      .WillByDefault(Return(task));

  // Exit zero state; enter normal state.
  EXPECT_CALL(delegate, OnZeroStateChange(false));
  auto handle_query = CreateMockNavigationHandle(query_url);
  handle_query->set_has_committed(true);
  observer->DidFinishNavigation(handle_query.get());

  // Simulate full refresh. Enter zero state again.
  EXPECT_CALL(delegate, OnZeroStateChange(true));
  auto handle_zero = CreateMockNavigationHandle(zero_state_url);
  handle_zero->set_has_committed(true);
  observer->DidFinishNavigation(handle_zero.get());

  // Exit zero state; enter normal state again.
  EXPECT_CALL(delegate, OnZeroStateChange(false));
  auto handle_query2 = CreateMockNavigationHandle(query_url);
  handle_query2->set_has_committed(true);
  observer->DidFinishNavigation(handle_query2.get());
}

TEST_F(ContextualTasksUiTest,
       OnZeroStateChange_SameDocument_ZeroStateChanged_FeatureEnabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(
      contextual_tasks::kEnableNotifyZeroStateRenderedCapability);

  testing::NiceMock<MockTaskInfoDelegate> delegate;
  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);
  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL zero_state_url("https://www.google.com/search?udm=50");
  GURL query_url("https://www.google.com/search?udm=50&q=test");

  // First navigate to a non-zero state URL to set the baseline state.
  {
    EXPECT_CALL(delegate, OnZeroStateChange(false)).Times(1);
    auto handle = CreateMockNavigationHandle(query_url);
    handle->set_has_committed(true);
    handle->set_is_same_document(false);
    observer->DidFinishNavigation(handle.get());
  }

  // Now simulate a same-document navigation to a zero state URL.
  // Even though it's same-document and the feature is enabled,
  // OnZeroStateChange should be called because the zero state status has
  // changed (from false to true).
  {
    EXPECT_CALL(delegate, OnZeroStateChange(true)).Times(1);

    base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
    ContextualTask task(task_id);
    EXPECT_CALL(*contextual_tasks_service_, CreateTask())
        .WillOnce(Return(task));
    EXPECT_CALL(delegate, PrepareForTaskChange()).Times(1);
    EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, Optional(task_id), _))
        .Times(1);

    auto handle = CreateMockNavigationHandle(zero_state_url);
    handle->set_has_committed(true);
    handle->set_is_same_document(true);
    observer->DidFinishNavigation(handle.get());
  }
}

TEST_F(ContextualTasksUiTest,
       OnZeroStateChange_SameDocument_ZeroStateChanged_FeatureDisabled) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(
      contextual_tasks::kEnableNotifyZeroStateRenderedCapability);

  testing::NiceMock<MockTaskInfoDelegate> delegate;
  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);
  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL zero_state_url("https://www.google.com/search?udm=50");
  GURL query_url("https://www.google.com/search?udm=50&q=test");

  // First navigate to a non-zero state URL to set the baseline state.
  {
    EXPECT_CALL(delegate, OnZeroStateChange(false)).Times(1);
    auto handle = CreateMockNavigationHandle(query_url);
    handle->set_has_committed(true);
    handle->set_is_same_document(false);
    observer->DidFinishNavigation(handle.get());
  }

  // Now simulate a same-document navigation to a zero state URL.
  // Even though it's same-document, OnZeroStateChange should be called because
  // the zero state status has changed (from false to true).
  {
    EXPECT_CALL(delegate, OnZeroStateChange(true)).Times(1);

    base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
    ContextualTask task(task_id);
    EXPECT_CALL(*contextual_tasks_service_, CreateTask())
        .WillOnce(Return(task));
    EXPECT_CALL(delegate, PrepareForTaskChange()).Times(1);
    EXPECT_CALL(*service_for_nav_, OnTaskChanged(_, _, _, Optional(task_id), _))
        .Times(1);

    auto handle = CreateMockNavigationHandle(zero_state_url);
    handle->set_has_committed(true);
    handle->set_is_same_document(true);
    observer->DidFinishNavigation(handle.get());
  }
}

TEST_F(ContextualTasksUiTest, SetAimUrlWithoutThreadId) {
  GURL query_url("https://www.google.com/search?udm=50&q=test");
  testing::NiceMock<MockTaskInfoDelegate> delegate;
  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);
  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  // SetAimUrl() should be called even if mtid is missing since pre-prod server
  // may not have it.
  auto handle = CreateMockNavigationHandle(query_url);
  handle->set_has_committed(true);
  handle->set_is_same_document(false);
  observer->DidFinishNavigation(handle.get());
}

TEST_F(ContextualTasksUiTest, SetComposeboxHandler) {
  content::TestWebUI web_ui;
  web_ui.set_web_contents(embedded_web_contents_.get());
  ContextualTasksUI controller(&web_ui);

  testing::NiceMock<MockContextualTasksPage> page;
  mojo::PendingReceiver<mojom::PageHandler> handler_receiver;
  controller.CreatePageHandler(page.BindAndGetRemote(),
                               std::move(handler_receiver));

  auto handler = std::make_unique<MockContextualTasksComposeboxHandler>();
  auto* handler_ptr = handler.get();

  controller.SetComposeboxHandler(handler_ptr);

  // We can't easily verify the internal state since it's private, but we can
  // call a method that uses it.
  EXPECT_CALL(*handler_ptr, InitializeInputStateModel()).Times(1);
  controller.SetTaskId(base::Uuid::GenerateRandomV4());

  // Reset the handler in the controller before it goes out of scope to avoid
  // dangling pointer.
  controller.SetComposeboxHandler(nullptr);
}

TEST_F(ContextualTasksUiTest, OnWebUIReadyCalledOnInitComplete) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  GURL url(chrome::kChromeUIContextualTasksURL);
  url = net::AppendQueryParameter(url, kTaskQueryParam,
                                  task_id.AsLowercaseString());
  content::WebContentsTester::For(embedded_web_contents_.get())
      ->NavigateAndCommit(url);

  content::TestWebUI web_ui;
  web_ui.set_web_contents(embedded_web_contents_.get());

  ContextualTasksUI controller(&web_ui);

  // The signal should NOT be sent upon construction.
  EXPECT_CALL(*service_for_nav_, OnWebUIReady(_, _, _)).Times(0);
  testing::Mock::VerifyAndClearExpectations(service_for_nav_);

  // The signal SHOULD be sent upon CreatePageHandler (which calls
  // OnInitComplete).
  EXPECT_CALL(*service_for_nav_, OnWebUIReady(_, task_id, _)).Times(1);
  // Expect OnWebUIDestroyed when controller goes out of scope.
  EXPECT_CALL(*service_for_nav_, OnWebUIDestroyed(_, std::optional(task_id)))
      .Times(1);

  testing::NiceMock<MockContextualTasksPage> page;
  mojo::PendingReceiver<mojom::PageHandler> handler_receiver;
  controller.CreatePageHandler(page.BindAndGetRemote(),
                               std::move(handler_receiver));
}

class MockMPArchNavigationHandle : public content::MockNavigationHandle {
 public:
  MockMPArchNavigationHandle() = default;
  ~MockMPArchNavigationHandle() override = default;

  bool IsGuestViewMainFrame() const override { return is_guest_view_; }
  void set_is_guest_view_main_frame(bool is_guest_view) {
    is_guest_view_ = is_guest_view;
  }

 private:
  bool is_guest_view_ = false;
};

TEST_F(ContextualTasksUiTest, FrameNavObserver_DidFinishNavigation_MPArch) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kGuestViewMPArch);

  testing::NiceMock<MockTaskInfoDelegate> delegate;
  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);
  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);

  GURL url(kAiPageUrl);
  url = net::AppendQueryParameter(url, "q", "test");
  url = net::AppendQueryParameter(url, "mtid", "5678");

  // Simulate an MPArch guest main frame navigation.
  auto handle = std::make_unique<MockMPArchNavigationHandle>();
  handle->set_url(url);
  handle->set_has_committed(true);
  handle->set_is_guest_view_main_frame(true);

  EXPECT_CALL(*contextual_tasks_service_, GetTaskFromServerId(_, "5678"))
      .Times(1);
  EXPECT_CALL(*contextual_tasks_service_, CreateTaskFromUrl(url))
      .WillOnce(
          Return(ContextualTask(base::Uuid::ParseCaseInsensitive(kUuid))));

  observer->DidFinishNavigation(handle.get());

  // Simulate a top-level navigation.
  auto top_level_handle = std::make_unique<MockMPArchNavigationHandle>();
  top_level_handle->set_url(url);
  top_level_handle->set_has_committed(true);
  top_level_handle->set_is_guest_view_main_frame(false);

  // No interaction with the service should occur since top-level navs are
  // filtered out.
  EXPECT_CALL(*contextual_tasks_service_, GetTaskFromServerId(_, "5678"))
      .Times(0);

  observer->DidFinishNavigation(top_level_handle.get());
}

TEST_F(ContextualTasksUiTest, DidFinishNavigation_UpdatesThemeFromCsParam) {
  MockTaskInfoDelegate delegate;
  SetupMockDelegate(&delegate, std::nullopt, std::nullopt, std::nullopt);
  auto observer = std::make_unique<ContextualTasksUI::FrameNavObserver>(
      embedded_web_contents_.get(), service_for_nav_.get(),
      contextual_tasks_service_.get(), &delegate);
  GURL url("https://www.google.com/search?udm=50&cs=1");
  content::WebContents* wc = embedded_web_contents_.get();
  blink::web_pref::WebPreferences prefs = wc->GetOrCreateWebPreferences();
  // Initialize to light mode to verify it changes to dark.
  prefs.preferred_color_scheme = blink::mojom::PreferredColorScheme::kLight;
  wc->SetWebPreferences(prefs);
  base::Uuid task_id = base::Uuid::ParseCaseInsensitive(kUuid);
  ContextualTask task(task_id);
  ON_CALL(*contextual_tasks_service_, CreateTask()).WillByDefault(Return(task));
  auto handle = CreateMockNavigationHandle(url);
  handle->set_has_committed(true);
  handle->set_is_same_document(true);
  observer->DidFinishNavigation(handle.get());
  blink::web_pref::WebPreferences updated_prefs =
      wc->GetOrCreateWebPreferences();
  EXPECT_EQ(updated_prefs.preferred_color_scheme,
            blink::mojom::PreferredColorScheme::kDark);
}

}  // namespace contextual_tasks
