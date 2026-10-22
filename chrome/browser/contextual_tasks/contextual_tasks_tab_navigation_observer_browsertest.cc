// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_tab_navigation_observer.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

constexpr char kAiModeUrl[] = "https://www.google.com/search?udm=50&q=a";

}  // namespace

// Tests that full-tab AI Mode maps its thread (`mtid`) to the tab's task.
class ContextualTasksTabNavigationObserverBrowserTest
    : public InProcessBrowserTest {
 public:
  ContextualTasksTabNavigationObserverBrowserTest() {
    feature_list_.InitWithFeatures(
        {kContextualTasks, kContextualTasksRearchitecture}, {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContextualTasksTabNavigationObserverBrowserTest::
                    OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    // Serves all google.com URLs so that navigations commit.
    url_loader_interceptor_ =
        std::make_unique<content::URLLoaderInterceptor>(base::BindRepeating(
            [](content::URLLoaderInterceptor::RequestParams* params) {
              if (params->url_request.url.host() != "www.google.com") {
                return false;
              }
              content::URLLoaderInterceptor::WriteResponse(
                  "HTTP/1.1 200 OK\nContent-type: text/html\n\n",
                  "<html><body></body></html>", params->client.get());
              return true;
            }));
    web_contents_ =
        TabListInterface::From(browser())->GetActiveTab()->GetContents();
  }

  void TearDownOnMainThread() override {
    web_contents_ = nullptr;
    url_loader_interceptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          Profile* profile = Profile::FromBrowserContext(context);
          auto ui_service =
              std::make_unique<testing::NiceMock<MockContextualTasksUiService>>(
                  profile,
                  ContextualTasksServiceFactory::GetForProfile(profile),
                  IdentityManagerFactory::GetForProfile(profile),
                  AimEligibilityServiceFactory::GetForProfile(profile),
                  /*eligibility_manager=*/nullptr,
                  /*cookie_synchronizer=*/nullptr);
          ON_CALL(*ui_service, IsAiUrl(testing::_))
              .WillByDefault(
                  [](const GURL& url) { return url.path() == "/search"; });
          return ui_service;
        }));
  }

  ContextualTasksService* tasks_service() {
    return ContextualTasksServiceFactory::GetForProfile(
        browser()->GetProfile());
  }

  ContextualTasksWebContentsUserData* user_data() {
    return ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
        web_contents_);
  }

  std::optional<base::Uuid> GetTaskForThread(const std::string& thread_id) {
    std::optional<ContextualTask> task =
        tasks_service()->GetTaskFromServerId(ThreadType::kAiMode, thread_id);
    return task ? std::make_optional(task->GetTaskId()) : std::nullopt;
  }

  std::optional<base::Uuid> GetTaskForTab() {
    std::optional<ContextualTask> task =
        tasks_service()->GetContextualTaskForTab(
            sessions::SessionTabHelper::IdForTab(web_contents_));
    return task ? std::make_optional(task->GetTaskId()) : std::nullopt;
  }

  ContextualSearchWebContentsHelper* helper() {
    return ContextualSearchWebContentsHelper::FromWebContents(web_contents_);
  }

  ContextualTasksPanelController* panel_controller() {
    return ContextualTasksPanelController::From(browser());
  }

  // Navigates to `kAiModeUrl` plus `params`.
  void Navigate(const std::string& params) {
    ASSERT_TRUE(
        ui_test_utils::NavigateToURL(browser(), GURL(kAiModeUrl + params)));
  }

  // Updates the URL to `kAiModeUrl` plus `params` like the AI Mode page does,
  // with `history.replaceState()` (or `pushState()`).
  void UpdateUrl(const std::string& params, bool push = false) {
    content::TestNavigationObserver observer(web_contents_);
    ASSERT_TRUE(content::ExecJs(
        web_contents_, std::string("history.") +
                           (push ? "pushState" : "replaceState") +
                           "(null, '', '" + kAiModeUrl + params + "')"));
    observer.Wait();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksTabNavigationObserverBrowserTest,
                       ThreadSetsTabTask) {
  Navigate("&mtid=t1");

  std::optional<base::Uuid> task_id = GetTaskForThread("t1");
  ASSERT_TRUE(task_id);
  EXPECT_EQ(user_data()->task_id(), task_id);
  EXPECT_EQ(user_data()->GetEffectiveTaskId(), task_id);
  EXPECT_EQ(helper()->task_id(), task_id);
  EXPECT_EQ(GetTaskForTab(), task_id);

  // Reloading keeps the task and the session.
  contextual_search::ContextualSearchSessionHandle* session_handle =
      helper()->session_handle();
  ASSERT_TRUE(session_handle);
  EXPECT_EQ(panel_controller()->GetSessionHandleForActiveTabOrPanel(),
            std::make_pair(task_id, session_handle));
  Navigate("&mtid=t1");
  EXPECT_EQ(user_data()->task_id(), task_id);
  EXPECT_EQ(helper()->task_id(), task_id);
  EXPECT_EQ(helper()->session_handle(), session_handle);
  EXPECT_EQ(GetTaskForTab(), task_id);
  EXPECT_EQ(panel_controller()->GetSessionHandleForActiveTabOrPanel(),
            std::make_pair(task_id, session_handle));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksTabNavigationObserverBrowserTest,
                       ThreadChangeSwitchesTask) {
  Navigate("&mtid=t1");
  std::optional<base::Uuid> task_id1 = GetTaskForThread("t1");
  ASSERT_TRUE(task_id1);
  contextual_search::ContextualSearchSessionHandle* session_handle =
      helper()->session_handle();
  ASSERT_TRUE(session_handle);

  // E.g. a thread selected from history.
  UpdateUrl("&mtid=t2", /*push=*/true);
  std::optional<base::Uuid> task_id2 = GetTaskForThread("t2");
  ASSERT_TRUE(task_id2);
  EXPECT_NE(task_id2, task_id1);
  EXPECT_EQ(user_data()->GetEffectiveTaskId(), task_id2);
  EXPECT_EQ(helper()->task_id(), task_id2);
  EXPECT_EQ(helper()->session_handle(), session_handle);
  EXPECT_EQ(GetTaskForTab(), task_id2);
  EXPECT_THAT(tasks_service()->GetTabsAssociatedWithTask(*task_id1),
              testing::IsEmpty());

  // Going back returns to the previous thread's task.
  content::TestNavigationObserver observer(web_contents_);
  web_contents_->GetController().GoBack();
  observer.Wait();
  EXPECT_EQ(user_data()->GetEffectiveTaskId(), task_id1);
  EXPECT_EQ(GetTaskForTab(), task_id1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksTabNavigationObserverBrowserTest,
                       ZeroStateHasNoTaskAndKeepsAttachedTabs) {
  Navigate("");
  EXPECT_FALSE(user_data()->GetEffectiveTaskId());
  EXPECT_FALSE(GetTaskForTab());
  EXPECT_EQ(panel_controller()->GetSessionHandleForActiveTabOrPanel(),
            std::make_pair(std::optional<base::Uuid>(), nullptr));

  // A tab attached before the first response.
  contextual_search::ContextualSearchSessionHandle* session_handle =
      user_data()->GetOrCreateContextualSessionHandle();
  ASSERT_TRUE(session_handle);
  session_handle->AddPendingDelayedTab(123, GURL("https://example.com"),
                                       "Example");

  UpdateUrl("&mtid=t1");
  std::optional<base::Uuid> task_id1 = GetTaskForThread("t1");
  ASSERT_TRUE(task_id1);
  EXPECT_EQ(user_data()->GetEffectiveTaskId(), task_id1);
  EXPECT_EQ(GetTaskForTab(), task_id1);
  EXPECT_EQ(helper()->session_handle(), session_handle);
  EXPECT_EQ(panel_controller()->GetSessionHandleForActiveTabOrPanel(),
            std::make_pair(task_id1, session_handle));
  EXPECT_EQ(session_handle->GetTabContextState().attached.size(), 1u);

  // A new thread is in zero state until its first response.
  UpdateUrl("");
  EXPECT_FALSE(user_data()->GetEffectiveTaskId());
  EXPECT_FALSE(GetTaskForTab());
  EXPECT_FALSE(user_data()->task_id());
  EXPECT_EQ(panel_controller()->GetSessionHandleForActiveTabOrPanel(),
            std::make_pair(std::optional<base::Uuid>(), nullptr));

  UpdateUrl("&mtid=t2");
  std::optional<base::Uuid> task_id2 = GetTaskForThread("t2");
  ASSERT_TRUE(task_id2);
  EXPECT_NE(task_id2, task_id1);
  EXPECT_EQ(user_data()->GetEffectiveTaskId(), task_id2);
  EXPECT_EQ(helper()->session_handle(), session_handle);
  EXPECT_EQ(panel_controller()->GetSessionHandleForActiveTabOrPanel(),
            std::make_pair(task_id2, session_handle));
  EXPECT_EQ(session_handle->GetTabContextState().attached.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksTabNavigationObserverBrowserTest,
                       NonAiNavigationClearsTask) {
  Navigate("&mtid=t1");
  ASSERT_TRUE(user_data()->GetEffectiveTaskId());

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("https://www.google.com/other")));
  EXPECT_FALSE(user_data()->task_id());
  EXPECT_FALSE(user_data()->GetEffectiveTaskId());
  EXPECT_FALSE(GetTaskForTab());
  EXPECT_EQ(panel_controller()->GetSessionHandleForActiveTabOrPanel(),
            std::make_pair(std::optional<base::Uuid>(), nullptr));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksTabNavigationObserverBrowserTest,
                       BackgroundTabThreadUpdateSetsTask) {
  Navigate("");
  ASSERT_FALSE(user_data()->GetEffectiveTaskId());

  // Switch to another foreground tab while the AI Mode tab updates `mtid` in
  // the background.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com/other"),
      WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_NE(TabListInterface::From(browser())->GetActiveTab()->GetContents(),
            web_contents_);

  UpdateUrl("&mtid=t1");
  std::optional<base::Uuid> task_id = GetTaskForThread("t1");
  ASSERT_TRUE(task_id);
  EXPECT_EQ(user_data()->GetEffectiveTaskId(), task_id);
  EXPECT_EQ(GetTaskForTab(), task_id);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksTabNavigationObserverBrowserTest,
                       NoTaskOutsideTab) {
  // E.g. the side panel, whose task is set by `ContextualTasksUiService`.
  std::unique_ptr<content::WebContents> web_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->GetProfile()));
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents.get());
  ASSERT_TRUE(content::NavigateToURL(
      web_contents.get(), GURL(std::string(kAiModeUrl) + "&mtid=t1")));
  EXPECT_FALSE(user_data->GetEffectiveTaskId());
  EXPECT_FALSE(GetTaskForThread("t1"));
}

}  // namespace contextual_tasks
