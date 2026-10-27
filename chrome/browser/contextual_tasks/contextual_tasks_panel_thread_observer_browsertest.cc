// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_panel_thread_observer.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/uuid.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_eligibility_manager.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "url/gurl.h"

namespace contextual_tasks {

namespace {

constexpr char kZeroStateUrl[] = "https://www.google.com/search?udm=50";
constexpr char kAiModeUrl[] = "https://www.google.com/search?udm=50&q=a";

using ::testing::Ne;
using ::testing::Optional;

}  // namespace

// Tests that the side panel's AI Mode thread (`mtid`) is kept in sync with the
// panel's task.
class ContextualTasksPanelThreadObserverBrowserTest
    : public InProcessBrowserTest {
 public:
  ContextualTasksPanelThreadObserverBrowserTest() {
    feature_list_.InitWithFeatures(
        {kContextualTasks, kContextualTasksRearchitecture}, {});
  }

  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &ContextualTasksPanelThreadObserverBrowserTest::
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

    // Like the side panel's AI Mode `WebContents`, whose initial task is set
    // by `ContextualTasksUiService`.
    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(browser()->GetProfile()));
    webui::SetBrowserWindowInterface(web_contents_.get(), browser());
    initial_task_id_ = tasks_service()->CreateTask().GetTaskId();
    SetTaskSession(web_contents_.get(), initial_task_id_);
    ContextualTasksPanelThreadObserver::CreateForWebContents(
        web_contents_.get());

    // Like `ContextualTasksSidePanelCoordinator::OnTaskChanged()`, which keeps
    // the panel's session.
    ON_CALL(ui_service(), OnTaskChanged)
        .WillByDefault([](BrowserWindowInterface* /*browser_window_interface*/,
                          content::WebContents* web_contents,
                          const std::optional<base::Uuid>& /*old_task_id*/,
                          const std::optional<base::Uuid>& new_task_id,
                          bool /*is_shown_in_tab*/) {
          ContextualSearchWebContentsHelper* helper =
              ContextualSearchWebContentsHelper::FromWebContents(web_contents);
          helper->SetTaskSession(new_task_id, helper->TakeSessionHandle(),
                                 helper->TakeInputStateModel());
        });
  }

  void TearDownOnMainThread() override {
    web_contents_.reset();
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

  // Sets the task and a new session of `contents`'s
  // `ContextualSearchWebContentsHelper`, like
  // `UpdateContextualSearchWebContentsHelperForTask()` does for the panel.
  void SetTaskSession(content::WebContents* contents,
                      std::optional<base::Uuid> task_id) {
    std::unique_ptr<contextual_search::ContextualSearchSessionHandle> session =
        ContextualSearchServiceFactory::GetForProfile(browser()->GetProfile())
            ->CreateSession(
                CreateQueryControllerConfigParams(),
                contextual_search::ContextualSearchSource::kContextualTasks,
                lens::LensOverlayInvocationSource::kContextualTasksComposebox);
    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(contents)
        ->SetTaskSession(task_id, std::move(session),
                         /*input_state_model=*/nullptr);
  }

  ContextualTasksService* tasks_service() {
    return ContextualTasksServiceFactory::GetForProfile(
        browser()->GetProfile());
  }

  testing::NiceMock<MockContextualTasksUiService>& ui_service() {
    return *static_cast<testing::NiceMock<MockContextualTasksUiService>*>(
        ContextualTasksUiServiceFactory::GetForBrowserContext(
            browser()->GetProfile()));
  }

  std::optional<base::Uuid> GetTaskForThread(const std::string& thread_id) {
    std::optional<ContextualTask> task =
        tasks_service()->GetTaskFromServerId(ThreadType::kAiMode, thread_id);
    return task ? std::make_optional(task->GetTaskId()) : std::nullopt;
  }

  std::optional<base::Uuid> GetPanelTask() {
    return ContextualSearchWebContentsHelper::FromWebContents(
               web_contents_.get())
        ->task_id();
  }

  void Navigate(const std::string& url) {
    ASSERT_TRUE(content::NavigateToURL(web_contents_.get(), GURL(url)));
  }

  // Updates the URL of `contents` to `url` like the AI Mode page does, with
  // `history.replaceState()` (or `pushState()`).
  void UpdateUrl(content::WebContents* contents,
                 const std::string& url,
                 bool push = false) {
    content::TestNavigationObserver observer(contents);
    ASSERT_TRUE(
        content::ExecJs(contents, std::string("history.") +
                                      (push ? "pushState" : "replaceState") +
                                      "(null, '', '" + url + "')"));
    observer.Wait();
  }
  void UpdateUrl(const std::string& url, bool push = false) {
    UpdateUrl(web_contents_.get(), url, push);
  }

  // Sends `OnPageChanged` from the AI Mode page in `contents`.
  void SendOnPageChanged(content::WebContents* contents, bool is_zero_state) {
    lens::SearchToClientMessage message;
    message.mutable_on_page_changed()->set_is_zero_state(is_zero_state);
    std::vector<uint8_t> bytes(message.ByteSizeLong());
    message.SerializeToArray(bytes.data(), bytes.size());
    ASSERT_TRUE(
        ContextualTasksWebContentsUserData::GetOrCreateForWebContents(contents)
            ->OnSearchMessageReceived(bytes));
  }

  // Like "New chat": the AI Mode page goes to zero state and reports it.
  void StartNewChat(content::WebContents* contents) {
    UpdateUrl(contents, kZeroStateUrl, /*push=*/true);
    SendOnPageChanged(contents, /*is_zero_state=*/true);
  }
  void StartNewChat() { StartNewChat(web_contents_.get()); }

 protected:
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription create_services_subscription_;
  std::unique_ptr<content::URLLoaderInterceptor> url_loader_interceptor_;
  std::unique_ptr<content::WebContents> web_contents_;
  base::Uuid initial_task_id_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksPanelThreadObserverBrowserTest,
                       ThreadIsAddedToPanelTask) {
  EXPECT_CALL(ui_service(), OnTaskChanged).Times(0);
  Navigate(kZeroStateUrl);
  UpdateUrl(std::string(kAiModeUrl) + "&mstk=turn1");
  EXPECT_FALSE(GetTaskForThread("t1"));

  // The first response adds the thread to the panel's task.
  UpdateUrl(std::string(kAiModeUrl) + "&mstk=turn1&mtid=t1");
  EXPECT_EQ(GetTaskForThread("t1"), initial_task_id_);
  EXPECT_EQ(GetPanelTask(), initial_task_id_);

  // Later turns stay on the task.
  UpdateUrl(std::string(kAiModeUrl) + "&mstk=turn2&mtid=t1");
  EXPECT_EQ(GetTaskForThread("t1"), initial_task_id_);
  EXPECT_EQ(GetPanelTask(), initial_task_id_);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPanelThreadObserverBrowserTest,
                       NewChatMovesToNewTask) {
  Navigate(std::string(kAiModeUrl) + "&mtid=t1");
  ASSERT_EQ(GetTaskForThread("t1"), initial_task_id_);

  // "New chat" moves to a new task before the new thread's first submit.
  EXPECT_CALL(ui_service(), OnTaskChanged(browser(), web_contents_.get(),
                                          Optional(initial_task_id_),
                                          Optional(Ne(initial_task_id_)),
                                          /*is_shown_in_tab=*/false));
  StartNewChat();
  std::optional<base::Uuid> new_task_id = GetPanelTask();
  ASSERT_TRUE(new_task_id);
  EXPECT_NE(new_task_id, initial_task_id_);
  testing::Mock::VerifyAndClearExpectations(&ui_service());

  // The new thread is added to the new task, and the previous thread keeps its
  // task.
  EXPECT_CALL(ui_service(), OnTaskChanged).Times(0);
  UpdateUrl(std::string(kAiModeUrl) + "&mtid=t2");
  EXPECT_EQ(GetTaskForThread("t2"), new_task_id);
  EXPECT_EQ(GetTaskForThread("t1"), initial_task_id_);
  EXPECT_EQ(GetPanelTask(), new_task_id);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPanelThreadObserverBrowserTest,
                       ZeroStateWithoutThreadKeepsTask) {
  EXPECT_CALL(ui_service(), OnTaskChanged).Times(0);
  Navigate(kZeroStateUrl);
  SendOnPageChanged(web_contents_.get(), /*is_zero_state=*/true);
  EXPECT_EQ(GetPanelTask(), initial_task_id_);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPanelThreadObserverBrowserTest,
                       NewChatWithoutThreadMovesToNewTask) {
  // E.g. signed-out AI Mode, whose turns have `mstk` but whose threads have no
  // `mtid`.
  Navigate(kZeroStateUrl);
  UpdateUrl(std::string(kAiModeUrl) + "&mstk=turn1");

  EXPECT_CALL(ui_service(), OnTaskChanged(browser(), web_contents_.get(),
                                          Optional(initial_task_id_),
                                          Optional(Ne(initial_task_id_)),
                                          /*is_shown_in_tab=*/false));
  StartNewChat();
  std::optional<base::Uuid> new_task_id = GetPanelTask();
  ASSERT_TRUE(new_task_id);
  EXPECT_NE(new_task_id, initial_task_id_);
  testing::Mock::VerifyAndClearExpectations(&ui_service());

  // The new task is in zero state, so reporting zero state again keeps it.
  EXPECT_CALL(ui_service(), OnTaskChanged).Times(0);
  SendOnPageChanged(web_contents_.get(), /*is_zero_state=*/true);
  EXPECT_EQ(GetPanelTask(), new_task_id);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPanelThreadObserverBrowserTest,
                       OnlyZeroStatePageChangeMovesToNewTask) {
  Navigate(std::string(kAiModeUrl) + "&mtid=t1");

  // Neither a zero state URL without `OnPageChanged` nor a non-zero state
  // `OnPageChanged` starts a new task.
  EXPECT_CALL(ui_service(), OnTaskChanged).Times(0);
  UpdateUrl(kZeroStateUrl, /*push=*/true);
  SendOnPageChanged(web_contents_.get(), /*is_zero_state=*/false);
  EXPECT_EQ(GetPanelTask(), initial_task_id_);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPanelThreadObserverBrowserTest,
                       ZeroStateThreadIsAddedToCurrentTask) {
  Navigate(kZeroStateUrl);

  // E.g. a thread picked from history before the panel's first submit. Like
  // for `ContextualTasksTabNavigationObserver`, the thread gets the zero state
  // task with its attached tabs.
  EXPECT_CALL(ui_service(), OnTaskChanged).Times(0);
  UpdateUrl(std::string(kAiModeUrl) + "&mtid=t1", /*push=*/true);
  EXPECT_EQ(GetTaskForThread("t1"), initial_task_id_);
  EXPECT_EQ(GetPanelTask(), initial_task_id_);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPanelThreadObserverBrowserTest,
                       WebContentsMovedFromTabUsesItsTask) {
  // Like a full-tab AI Mode `WebContents` on thread "t1" moved to the panel:
  // its task is in `ContextualTasksWebContentsUserData` rather than in its
  // `ContextualSearchWebContentsHelper`.
  std::unique_ptr<content::WebContents> moved_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->GetProfile()));
  webui::SetBrowserWindowInterface(moved_contents.get(), browser());
  const base::Uuid tab_task_id = tasks_service()->CreateTask().GetTaskId();
  tasks_service()->UpdateThreadForTask(tab_task_id, ThreadType::kAiMode, "t1",
                                       std::nullopt, std::nullopt);
  ASSERT_TRUE(content::NavigateToURL(
      moved_contents.get(), GURL(std::string(kAiModeUrl) + "&mtid=t1")));
  ContextualTasksWebContentsUserData* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          moved_contents.get());
  user_data->SetTaskId(tab_task_id);
  SetTaskSession(moved_contents.get(), /*task_id=*/std::nullopt);
  ContextualTasksPanelThreadObserver::CreateForWebContents(
      moved_contents.get());

  // "New chat" moves from the tab's task to a new task.
  EXPECT_CALL(ui_service(),
              OnTaskChanged(browser(), moved_contents.get(),
                            Optional(tab_task_id), Optional(Ne(tab_task_id)),
                            /*is_shown_in_tab=*/false));
  StartNewChat(moved_contents.get());
  EXPECT_FALSE(user_data->task_id());
  EXPECT_EQ(GetTaskForThread("t1"), tab_task_id);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPanelThreadObserverBrowserTest,
                       ThreadWithTaskMovesToItsTask) {
  Navigate(std::string(kAiModeUrl) + "&mtid=t1");
  const base::Uuid other_task_id = tasks_service()->CreateTask().GetTaskId();
  tasks_service()->UpdateThreadForTask(other_task_id, ThreadType::kAiMode, "t2",
                                       std::nullopt, std::nullopt);

  // E.g. a thread picked from history.
  EXPECT_CALL(ui_service(),
              OnTaskChanged(browser(), web_contents_.get(),
                            Optional(initial_task_id_), Optional(other_task_id),
                            /*is_shown_in_tab=*/false));
  UpdateUrl(std::string(kAiModeUrl) + "&mtid=t2", /*push=*/true);
  EXPECT_EQ(GetPanelTask(), other_task_id);
  EXPECT_EQ(GetTaskForThread("t1"), initial_task_id_);
  EXPECT_EQ(GetTaskForThread("t2"), other_task_id);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPanelThreadObserverBrowserTest,
                       ThreadWithoutTaskMovesToNewTask) {
  Navigate(std::string(kAiModeUrl) + "&mtid=t1");

  // E.g. a thread from another device picked from history. Like for
  // `ContextualTasksTabNavigationObserver`, any other thread without a task
  // gets a new task.
  EXPECT_CALL(ui_service(), OnTaskChanged(browser(), web_contents_.get(),
                                          Optional(initial_task_id_),
                                          Optional(Ne(initial_task_id_)),
                                          /*is_shown_in_tab=*/false));
  UpdateUrl(std::string(kAiModeUrl) + "&mtid=t2");
  std::optional<base::Uuid> new_task_id = GetTaskForThread("t2");
  ASSERT_TRUE(new_task_id);
  EXPECT_NE(new_task_id, initial_task_id_);
  EXPECT_EQ(GetPanelTask(), new_task_id);
  EXPECT_EQ(GetTaskForThread("t1"), initial_task_id_);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksPanelThreadObserverBrowserTest,
                       TaskChangeClearsWebContentsUserDataTask) {
  // E.g. a `WebContents` moved from a full-tab AI Mode tab.
  ContextualTasksWebContentsUserData* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_.get());
  user_data->SetTaskId(initial_task_id_);
  Navigate(std::string(kAiModeUrl) + "&mtid=t1");

  StartNewChat();
  std::optional<base::Uuid> new_task_id = GetPanelTask();
  ASSERT_TRUE(new_task_id);
  EXPECT_NE(new_task_id, initial_task_id_);
  EXPECT_FALSE(user_data->task_id());
  EXPECT_EQ(user_data->GetEffectiveTaskId(), new_task_id);
}

}  // namespace contextual_tasks
