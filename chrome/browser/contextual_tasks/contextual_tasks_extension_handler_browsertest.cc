// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_handler.h"

#include <memory>
#include <variant>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/values.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_overlay_controller.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/lens/test_lens_overlay_query_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/cr_components/searchbox/contextual_searchbox_handler.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/permissions/permission_request_manager_test_api.h"
#include "components/contextual_search/contextual_search_types.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/permissions/request_type.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "mojo/public/cpp/base/proto_wrapper.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "third_party/lens_server_proto/lens_overlay_contextual_inputs.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/events/base_event_utils.h"

namespace contextual_tasks {

using testing::_;
using testing::NiceMock;
using testing::Return;

class MockLensSearchController : public LensSearchController {
 public:
  explicit MockLensSearchController(tabs::TabInterface* tab)
      : LensSearchController(tab) {}
  ~MockLensSearchController() override = default;

  MOCK_METHOD(void,
              OpenLensOverlay,
              (lens::LensOverlayInvocationSource invocation_source,
               bool should_show_csb),
              (override));
  MOCK_METHOD(void,
              CloseLensSync,
              (lens::LensOverlayDismissalSource dismissal_source),
              (override));
  MOCK_METHOD(void,
              CloseLensAsync,
              (lens::LensOverlayDismissalSource dismissal_source),
              (override));
  MOCK_METHOD(bool, IsShowingUI, (), (override));
  MOCK_METHOD(std::optional<lens::LensOverlayInvocationSource>,
              invocation_source,
              (),
              (override));
  MOCK_METHOD(void,
              CloseLensAsync,
              (lens::LensOverlayDismissalSource dismissal_source,
               bool side_panel_already_closing),
              (override));
  MOCK_METHOD(LensOverlayController*, lens_overlay_controller, (), (override));
  MOCK_METHOD(lens::LensOverlayQueryController*,
              lens_overlay_query_controller,
              (),
              (override));
  MOCK_METHOD(bool, IsCurrentTabSameOrigin, (), (const, override));
};

class MockLensOverlayController : public LensOverlayController {
 public:
  MockLensOverlayController(tabs::TabInterface* tab,
                            LensSearchController* search_controller,
                            Profile* profile)
      : LensOverlayController(tab, search_controller, profile->GetPrefs()) {}
  MockLensOverlayController(tabs::TabInterface* tab,
                            LensSearchController* search_controller,
                            PrefService* pref_service)
      : LensOverlayController(tab, search_controller, pref_service) {}
  ~MockLensOverlayController() override = default;

  MOCK_METHOD(void, ClearRegionSelection, (), (override));
  MOCK_METHOD(bool, HasRegionSelection, (), (const, override));
  const lens::mojom::CenterRotatedBoxPtr& selected_region() const override {
    return selected_region_;
  }
  const SkBitmap& initial_screenshot() const override {
    return initial_screenshot_.empty()
               ? LensOverlayController::initial_screenshot()
               : initial_screenshot_;
  }
  lens::mojom::CenterRotatedBoxPtr selected_region_;
  SkBitmap initial_screenshot_;
};

class ContextualTasksExtensionHandlerBrowserTestBase
    : public InProcessBrowserTest {
 public:
  ContextualTasksExtensionHandlerBrowserTestBase(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features) {
    feature_list_.InitWithFeatures(enabled_features, disabled_features);
    lens_controller_override_ =
        tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
            base::BindLambdaForTesting(
                [this](tabs::TabInterface& tab)
                    -> std::unique_ptr<LensSearchController> {
                  auto mock = std::make_unique<
                      testing::NiceMock<MockLensSearchController>>(&tab);
                  if (!this->mock_lens_controller_) {
                    this->mock_lens_controller_ = mock.get();
                  }
                  return mock;
                }));
  }
  ~ContextualTasksExtensionHandlerBrowserTestBase() override = default;

  void SimulateUserInteraction() {
    blink::WebMouseEvent mouse_event(blink::WebInputEvent::Type::kMouseDown,
                                     blink::WebInputEvent::kNoModifiers,
                                     ui::EventTimeForNow());
    mouse_event.button = blink::WebMouseEvent::Button::kLeft;
    web_contents_->GetPrimaryMainFrame()
        ->GetRenderWidgetHost()
        ->SimulateUserInteraction(mouse_event);
    ASSERT_TRUE(web_contents_->HasRecentInteraction());
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(chrome::kChromeUIVersionURL)));

    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();

    ContextualTasksExtensionHandler::CreateForCurrentDocument(rfh);
    handler_ = ContextualTasksExtensionHandler::GetForCurrentDocument(rfh);
    ASSERT_NE(handler_, nullptr);

    // Set up mock session handle and controller before binding handlers so
    // initial subscriptions attach to `mock_session_handle_`.
    auto session_handle = std::make_unique<
        NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
    mock_session_handle_ = session_handle.get();

    mock_controller_ = std::make_unique<
        NiceMock<contextual_search::MockContextualSearchContextController>>();

    ON_CALL(*mock_session_handle_, GetController())
        .WillByDefault(Return(mock_controller_.get()));

    lens::ClientToAimMessage non_empty_message;
    non_empty_message.mutable_submit_query();
    ON_CALL(*mock_controller_, CreateClientToAimRequest(_))
        .WillByDefault(Return(non_empty_message));

    // Set up file info for the active tab context.
    SessionID active_tab_id =
        sessions::SessionTabHelper::IdForTab(web_contents_);
    contextual_search::FileInfo file_info;
    file_info.tab_session_id = active_tab_id;
    lens::LensOverlayRequestId request_id;
    request_id.set_context_id(12345);
    file_info.request_id = request_id;
    std::vector<contextual_search::FileInfo> file_infos = {file_info};

    ON_CALL(*mock_session_handle_, CreateContextToken())
        .WillByDefault([this]() {
          auto token = base::UnguessableToken::Create();
          mock_session_handle_->GetUploadedContextTokensForTesting().push_back(
              token);
          return token;
        });

    ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
        .WillByDefault(Return(file_infos));

    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
        ->SetTaskSession(std::nullopt, std::move(session_handle),
                         /*input_state_model=*/nullptr);

    // Bind the mock page to the handler.
    mojo::PendingReceiver<mojom::ExtensionPageHandler> page_handler_receiver;
    handler_->CreateExtensionPageHandler(mock_page_.BindAndGetRemote(),
                                         std::move(page_handler_receiver));

    // Bind the mock searchbox page and composebox page handler to the handler.
    mojo::PendingReceiver<searchbox::mojom::PageHandler> searchbox_receiver;
    static_cast<composebox::mojom::PageHandlerFactory*>(handler_)
        ->CreatePageHandler(
            composebox_handler_remote_.BindNewPipeAndPassReceiver(),
            mock_searchbox_page_.BindAndGetRemote(),
            std::move(searchbox_receiver));

    mock_lens_overlay_controller_ =
        std::make_unique<NiceMock<MockLensOverlayController>>(
            browser()->tab_strip_model()->GetActiveTab(), mock_lens_controller_,
            Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
  }

  void TearDownOnMainThread() override {
    mock_lens_overlay_controller_.reset();
    if (mock_lens_controller_) {
      testing::Mock::VerifyAndClearExpectations(mock_lens_controller_);
      mock_lens_controller_ = nullptr;
    }
    composebox_handler_remote_.reset();
    handler_ = nullptr;
    if (side_panel_task_id_.has_value()) {
      ContextualTasksPanelController::From(browser())->DetachWebContentsForTask(
          *side_panel_task_id_);
      side_panel_task_id_.reset();
    }
    mock_session_handle_ = nullptr;
    mock_controller_.reset();
    web_contents_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

  void EmbedHandlerInSidePanel() {
    auto* tasks_service = ContextualTasksServiceFactory::GetForProfile(
        Profile::FromBrowserContext(web_contents_->GetBrowserContext()));
    ASSERT_NE(tasks_service, nullptr);
    ContextualTask task = tasks_service->CreateTask();
    side_panel_task_id_ = task.GetTaskId();
    tasks_service->AssociateTabWithTask(
        *side_panel_task_id_,
        sessions::SessionTabHelper::IdForTab(web_contents_));

    auto panel_contents = content::WebContents::Create(
        content::WebContents::CreateParams(web_contents_->GetBrowserContext()));
    content::WebContents* raw_panel_contents = panel_contents.get();
    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
        raw_panel_contents)
        ->SetTaskSession(
            *side_panel_task_id_,
            std::make_unique<NiceMock<
                contextual_search::MockContextualSearchSessionHandle>>(),
            /*input_state_model=*/nullptr);
    ContextualTasksPanelController::From(browser())->TransferWebContentsFromTab(
        *side_panel_task_id_, std::move(panel_contents));

    content::RenderFrameHost* rfh = raw_panel_contents->GetPrimaryMainFrame();
    ContextualTasksExtensionHandler::CreateForCurrentDocument(rfh);
    handler_ = ContextualTasksExtensionHandler::GetForCurrentDocument(rfh);
    ASSERT_NE(handler_, nullptr);

    mojo::PendingReceiver<mojom::ExtensionPageHandler> page_handler_receiver;
    handler_->CreateExtensionPageHandler(mock_panel_page_.BindAndGetRemote(),
                                         std::move(page_handler_receiver));

    composebox_handler_remote_.reset();
    mock_searchbox_page_.receiver_.reset();
    mojo::PendingReceiver<searchbox::mojom::PageHandler> searchbox_receiver;
    static_cast<composebox::mojom::PageHandlerFactory*>(handler_)
        ->CreatePageHandler(
            composebox_handler_remote_.BindNewPipeAndPassReceiver(),
            mock_searchbox_page_.BindAndGetRemote(),
            std::move(searchbox_receiver));
  }

  void AttachActiveTab(const base::UnguessableToken& token) {
    SessionID active_tab_id =
        sessions::SessionTabHelper::IdForTab(web_contents_);
    mock_session_handle_->GetUploadedContextTokensForTesting().push_back(token);
    mock_session_handle_->AddDelayedTabContext(
        token, active_tab_id.id(), web_contents_->GetLastCommittedURL(),
        "Test Tab");
  }

 protected:
  ui::UserDataFactory::ScopedOverride lens_controller_override_;
  raw_ptr<MockLensSearchController> mock_lens_controller_ = nullptr;
  std::unique_ptr<NiceMock<MockLensOverlayController>>
      mock_lens_overlay_controller_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<ContextualTasksExtensionHandler> handler_ = nullptr;
  std::optional<base::Uuid> side_panel_task_id_;
  NiceMock<MockContextualTasksExtensionPage> mock_page_;
  NiceMock<MockContextualTasksExtensionPage> mock_panel_page_;
  NiceMock<MockSearchboxPage> mock_searchbox_page_;
  raw_ptr<contextual_search::MockContextualSearchSessionHandle>
      mock_session_handle_ = nullptr;
  std::unique_ptr<contextual_search::MockContextualSearchContextController>
      mock_controller_;
  mojo::Remote<composebox::mojom::PageHandler> composebox_handler_remote_;
};

class ContextualTasksExtensionHandlerBrowserTest
    : public ContextualTasksExtensionHandlerBrowserTestBase {
 public:
  ContextualTasksExtensionHandlerBrowserTest()
      : ContextualTasksExtensionHandlerBrowserTestBase(
            {kContextualTasks, kContextualTasksRearchitecture,
             kContextualTasksForceEntryPointEligibility},
            {}) {}
};

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       GetInputState) {
  base::RunLoop run_loop;

  static_cast<searchbox::mojom::PageHandler*>(handler_)->GetInputState(
      base::BindLambdaForTesting(
          [&](const std::optional<omnibox::InputState>& state) {
            // Since we are mocking the session, we expect a valid model to be
            // created and a default InputState to be returned (not nullopt).
            EXPECT_TRUE(state.has_value());
            run_loop.Quit();
          }));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       OnPermissionPromptChangedPropagated) {
  test::PermissionRequestManagerTestApi test_api(browser());

  base::RunLoop show_run_loop;
  EXPECT_CALL(mock_searchbox_page_,
              OnPermissionPromptChanged(true, gfx::Size(0, 0)))
      .WillOnce(base::test::RunClosure(show_run_loop.QuitClosure()));

  test_api.AddSimpleRequest(web_contents_->GetPrimaryMainFrame(),
                            permissions::RequestType::kMultipleDownloads);
  show_run_loop.Run();

  base::RunLoop dismiss_run_loop;
  EXPECT_CALL(mock_searchbox_page_,
              OnPermissionPromptChanged(false, gfx::Size(0, 0)))
      .WillOnce(base::test::RunClosure(dismiss_run_loop.QuitClosure()));

  test_api.manager()->Dismiss(/*prompt_options=*/std::monostate());
  dismiss_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       HandleLensButtonClick) {
  EmbedHandlerInSidePanel();
  base::UserActionTester user_action_tester;

  ASSERT_TRUE(mock_lens_controller_);
  ASSERT_TRUE(composebox_handler_remote_.is_bound());

  EXPECT_CALL(*mock_lens_controller_, IsShowingUI())
      .WillRepeatedly(Return(false));

  base::RunLoop run_loop;
  EXPECT_CALL(
      *mock_lens_controller_,
      OpenLensOverlay(
          lens::LensOverlayInvocationSource::kContextualTasksComposebox, true))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  composebox_handler_remote_->HandleLensButtonClick();
  run_loop.Run();

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ContextualTasks.Composebox.UserAction.LensButtonClicked"));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       HandleLensButtonClick_NotInSidePanel_DoesNothing) {
  base::UserActionTester user_action_tester;

  ASSERT_TRUE(mock_lens_controller_);
  ASSERT_TRUE(composebox_handler_remote_.is_bound());

  EXPECT_CALL(*mock_lens_controller_, OpenLensOverlay(_, _)).Times(0);
  EXPECT_CALL(*mock_lens_controller_, CloseLensAsync(_)).Times(0);

  composebox_handler_remote_->HandleLensButtonClick();
  composebox_handler_remote_.FlushForTesting();

  EXPECT_EQ(0, user_action_tester.GetActionCount(
                   "ContextualTasks.Composebox.UserAction.LensButtonClicked"));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    HandleLensButtonClick_OverlayOpenFromComposebox_ClosesOverlay) {
  EmbedHandlerInSidePanel();
  base::UserActionTester user_action_tester;

  ASSERT_TRUE(mock_lens_controller_);
  ASSERT_TRUE(composebox_handler_remote_.is_bound());

  EXPECT_CALL(*mock_lens_controller_, IsShowingUI())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_lens_controller_, invocation_source())
      .WillRepeatedly(Return(
          lens::LensOverlayInvocationSource::kContextualTasksComposebox));

  base::RunLoop run_loop;
  EXPECT_CALL(*mock_lens_controller_,
              CloseLensAsync(lens::LensOverlayDismissalSource::
                                 kContextualTasksComposeboxLensButtonClick))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  composebox_handler_remote_->HandleLensButtonClick();
  run_loop.Run();

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ContextualTasks.Composebox.UserAction.LensButtonClicked"));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    HandleLensButtonClick_OverlayOpenFromOtherSource_UpdatesInvocationSourceAndOpens) {
  EmbedHandlerInSidePanel();
  base::UserActionTester user_action_tester;

  ASSERT_TRUE(mock_lens_controller_);
  ASSERT_TRUE(composebox_handler_remote_.is_bound());

  EXPECT_CALL(*mock_lens_controller_, IsShowingUI())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_lens_controller_, invocation_source())
      .WillRepeatedly(Return(lens::LensOverlayInvocationSource::kAppMenu));

  base::RunLoop run_loop;
  EXPECT_CALL(
      *mock_lens_controller_,
      OpenLensOverlay(
          lens::LensOverlayInvocationSource::kContextualTasksComposebox, true))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  composebox_handler_remote_->HandleLensButtonClick();
  run_loop.Run();

  EXPECT_EQ(1, user_action_tester.GetActionCount(
                   "ContextualTasks.Composebox.UserAction.LensButtonClicked"));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       OnLensOverlayStateChangedPropagated) {
  base::RunLoop show_run_loop;
  EXPECT_CALL(mock_page_, OnLensOverlayStateChanged(true))
      .WillOnce(base::test::RunClosure(show_run_loop.QuitClosure()));

  handler_->OnLensOverlayStateChanged(true);
  show_run_loop.Run();

  base::RunLoop hide_run_loop;
  EXPECT_CALL(mock_page_, OnLensOverlayStateChanged(false))
      .WillOnce(base::test::RunClosure(hide_run_loop.QuitClosure()));

  handler_->OnLensOverlayStateChanged(false);
  hide_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       GetRecentTabs) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com/test"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::RunLoop run_loop;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->GetRecentTabs(
      base::BindLambdaForTesting(
          [&](std::vector<searchbox::mojom::TabInfoPtr> tabs) {
            EXPECT_FALSE(tabs.empty());
            EXPECT_EQ(tabs[0]->url, GURL("https://www.google.com/test"));
            run_loop.Quit();
          }));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       GetRecentTabs_MultipleTabs) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com/test1"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com/test2"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  base::RunLoop run_loop;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->GetRecentTabs(
      base::BindLambdaForTesting(
          [&](std::vector<searchbox::mojom::TabInfoPtr> tabs) {
            EXPECT_GE(tabs.size(), 2u);
            EXPECT_EQ(tabs[0]->url, GURL("https://www.google.com/test2"));
            EXPECT_EQ(tabs[1]->url, GURL("https://www.google.com/test1"));
            run_loop.Quit();
          }));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       GetRecentTabs_NullWindowReturnsEmpty) {
  EXPECT_TRUE(ContextualSearchboxHandler::GetRecentTabInfos(nullptr).empty());
}

class ContextualTasksExtensionHandlerNoTabsBrowserTest
    : public ContextualTasksExtensionHandlerBrowserTest {
 public:
  void SetUpOnMainThread() override {
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), GURL(chrome::kChromeUIChromeURLsURL)));
    ContextualTasksExtensionHandlerBrowserTest::SetUpOnMainThread();
  }
};

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerNoTabsBrowserTest,
                       GetRecentTabs_NoEligibleTabs) {
  base::RunLoop run_loop;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->GetRecentTabs(
      base::BindLambdaForTesting(
          [&](std::vector<searchbox::mojom::TabInfoPtr> tabs) {
            EXPECT_TRUE(tabs.empty());
            run_loop.Quit();
          }));

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       AddAndDeleteTabContext) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t active_tab_id = active_tab->GetHandle().raw_value();

  base::RunLoop run_loop;
  base::RunLoop upload_run_loop;
  base::UnguessableToken token;

  EXPECT_CALL(*mock_session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([&](const base::UnguessableToken& file_token, auto, auto) {
        EXPECT_FALSE(file_token.is_empty());
        EXPECT_EQ(file_token, token);
        upload_run_loop.Quit();
      });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      active_tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            token = result.value();
            EXPECT_FALSE(token.is_empty());
            run_loop.Quit();
          }));
  run_loop.Run();
  upload_run_loop.Run();

  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      active_tab_id);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       AddAndDeleteContext) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t active_tab_id = active_tab->GetHandle().raw_value();

  base::RunLoop run_loop;
  base::RunLoop upload_run_loop;
  base::UnguessableToken token;

  EXPECT_CALL(*mock_session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([&](const base::UnguessableToken& file_token, auto, auto) {
        upload_run_loop.Quit();
      });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      active_tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            token = result.value();
            run_loop.Quit();
          }));
  run_loop.Run();
  upload_run_loop.Run();

  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteContext(
      token, /*from_automatic_chip=*/false);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest, ClearFiles) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t active_tab_id = active_tab->GetHandle().raw_value();

  base::RunLoop run_loop;
  base::RunLoop upload_run_loop;

  EXPECT_CALL(*mock_session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([&](auto, auto, auto) { upload_run_loop.Quit(); });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      active_tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();
  upload_run_loop.Run();

  static_cast<searchbox::mojom::PageHandler*>(handler_)->ClearFiles(
      /*should_block_auto_suggested_tabs=*/false);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       AddTabContext_AssociatesTabWithTask) {
  auto* contextual_tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(contextual_tasks_service, nullptr);
  ContextualTask task = contextual_tasks_service->CreateTask();
  handler_->SetTaskId(task.GetTaskId());
  EXPECT_TRUE(
      contextual_tasks_service->GetTabsAssociatedWithTask(task.GetTaskId())
          .empty());

  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t active_tab_id = active_tab->GetHandle().raw_value();
  SessionID active_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());

  base::RunLoop run_loop;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      active_tab_id, /*delay_upload=*/true,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Attaching the tab as context associates it with the handler's task.
  EXPECT_THAT(
      contextual_tasks_service->GetTabsAssociatedWithTask(task.GetTaskId()),
      testing::ElementsAre(active_session_id));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       AddTabContext_InvalidTabId) {
  base::RunLoop run_loop;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      99999, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting([&](base::expected<
                                     base::UnguessableToken,
                                     contextual_search::ContextUploadErrorType>
                                         result) {
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(
            result.error(),
            contextual_search::ContextUploadErrorType::kBrowserProcessingError);
        run_loop.Quit();
      }));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       AddTabContext_ReAddSameTabDeletesPreviousToken) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t active_tab_id = active_tab->GetHandle().raw_value();

  base::RunLoop first_run_loop;
  base::UnguessableToken first_token;

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      active_tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            first_token = result.value();
            first_run_loop.Quit();
          }));
  first_run_loop.Run();

  contextual_search::FileInfo dummy_info;
  ON_CALL(*mock_controller_, GetFileInfo(first_token))
      .WillByDefault(Return(&dummy_info));
  EXPECT_CALL(*mock_controller_, DeleteFile(first_token)).Times(1);

  base::RunLoop second_run_loop;
  base::RunLoop second_upload_run_loop;
  base::UnguessableToken second_token;

  EXPECT_CALL(*mock_session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([&](const base::UnguessableToken& file_token, auto, auto) {
        EXPECT_EQ(file_token, second_token);
        second_upload_run_loop.Quit();
      });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      active_tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            second_token = result.value();
            EXPECT_NE(first_token, second_token);
            second_run_loop.Quit();
          }));
  second_run_loop.Run();
  second_upload_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       AddTabContext_DeletedBeforeUploadCompletes) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t active_tab_id = active_tab->GetHandle().raw_value();

  EXPECT_CALL(*mock_session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  base::RunLoop run_loop;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      active_tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Immediately delete the tab context before async GetPageContext finishes.
  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      active_tab_id);

  // Wait for a subsequent GetPageContext on the same controller so the pending
  // extraction callback from AddTabContext executes and sees the deleted token.
  base::RunLoop flush_run_loop;
  lens::TabContextualizationController::From(active_tab)
      ->GetPageContext(base::BindLambdaForTesting(
          [&](std::unique_ptr<lens::ContextualInputData>) {
            flush_run_loop.Quit();
          }));
  flush_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       AddTabContext_DelayUploadSnapshotsUntilSubmit) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t active_tab_id = active_tab->GetHandle().raw_value();

  base::RunLoop add_run_loop;
  base::RunLoop snapshot_run_loop;
  base::UnguessableToken token;

  EXPECT_CALL(
      mock_searchbox_page_,
      OnContextualInputStatusChanged(
          testing::_, contextual_search::ContextUploadStatus::kProcessing,
          testing::_))
      .WillOnce([&](const base::UnguessableToken& status_token, auto, auto) {
        EXPECT_EQ(status_token, token);
        snapshot_run_loop.Quit();
      });

  // StartTabContextUploadFlow should not be called during AddTabContext when
  // delay_upload is true.
  EXPECT_CALL(*mock_session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      active_tab_id, /*delay_upload=*/true,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            token = result.value();
            add_run_loop.Quit();
          }));
  add_run_loop.Run();
  snapshot_run_loop.Run();

  testing::Mock::VerifyAndClearExpectations(mock_session_handle_);

  // Now submitting the query via OnWebviewMessage should trigger
  // StartTabContextUploadFlow with the snapshotted context.
  base::RunLoop submit_run_loop;
  EXPECT_CALL(*mock_session_handle_,
              StartTabContextUploadFlow(token, testing::_, testing::_))
      .Times(1);
  // Submission also refreshes the context library chip state, so only wait for
  // the on_submit_query_response message.
  EXPECT_CALL(mock_page_, PostSearchMessage(testing::_))
      .WillRepeatedly([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        if (message.has_value() && message->has_on_submit_query_response()) {
          submit_run_loop.Quit();
        }
      });

  lens::SearchToClientMessage search_message;
  search_message.mutable_on_submit_query_request();
  const size_t size = search_message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  search_message.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message);
  submit_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       DeleteTabContext_ClosedTabRemovesUnderline) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com/second_tab"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  tabs::TabInterface* second_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(second_tab, nullptr);
  int32_t second_tab_id = second_tab->GetHandle().raw_value();

  base::RunLoop run_loop;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      second_tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  // Close the second tab so its TabHandle resolves to nullptr.
  ASSERT_EQ(mock_session_handle_->GetTabContextState().attached.size(), 1u);
  int second_tab_index = browser()->tab_strip_model()->active_index();
  browser()->tab_strip_model()->DetachAndDeleteWebContentsAt(second_tab_index);
  ASSERT_EQ(tabs::TabHandle(second_tab_id).Get(), nullptr);

  // Deleting context for the closed tab should safely use the host's
  // BrowserWindowInterface and remove the closed tab from the session handle.
  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      second_tab_id);
  EXPECT_TRUE(mock_session_handle_->GetTabContextState().attached.empty());
  EXPECT_TRUE(mock_session_handle_->GetUploadedContextTokens().empty());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       TwoFramesResolveSameInputStateModel) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  handler_->SetTaskId(task_id);

  auto model1 = handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model1);

  // Set a lens crop through the primary handler and wait for the asynchronous
  // mount message (is_active=true) and context state update to reach the page.
  // Otherwise they may still be in flight when the unmount expectation below is
  // registered, and would be matched against it.
  base::RunLoop mount_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        const auto& inject_input = message->inject_chrome_input();
        EXPECT_EQ(inject_input.input_type(),
                  lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
        EXPECT_TRUE(inject_input.is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        mount_run_loop.Quit();
      });
  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,test_crop");
  mount_run_loop.Run();

  // Create a child iframe representing the lens chip extension frame.
  ASSERT_TRUE(
      content::ExecJs(web_contents_,
                      "const iframe = document.createElement('iframe'); "
                      "document.body.appendChild(iframe);"));
  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(web_contents_->GetPrimaryMainFrame(), 0);
  ASSERT_NE(child_rfh, nullptr);

  ContextualTasksExtensionHandler::CreateForCurrentDocument(child_rfh);
  auto* child_handler =
      ContextualTasksExtensionHandler::GetForCurrentDocument(child_rfh);
  ASSERT_NE(child_handler, nullptr);

  // Both handlers must resolve the same InputStateModel instance even though
  // the child handler does not have task_id set.
  auto model2 = child_handler->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model2);
  EXPECT_EQ(model1.get(), model2.get());

  // Verify child handler can read the crop via GetLensCropPreview.
  ASSERT_TRUE(model1->lens_crop().has_value());
  EXPECT_EQ("data:image/png;base64,test_crop", model1->lens_crop()->data_uri);

  base::RunLoop run_loop;
  child_handler->GetLensCropPreview(base::BindLambdaForTesting(
      [&](const std::optional<std::string>& data_uri) {
        ASSERT_TRUE(data_uri.has_value());
        EXPECT_EQ("data:image/png;base64,test_crop", *data_uri);
        run_loop.Quit();
      }));
  run_loop.Run();

  // Verify child handler calling RemoveLensCrop clears the model and notifies
  // the primary handler (which hosts lens_button) to emit InjectChromeInput
  // with is_active=false and OnContextStateChanged(CONTEXT_STATE_NONE) to AIM.
  base::RunLoop unmount_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        const auto& inject_input = message->inject_chrome_input();
        EXPECT_EQ(inject_input.input_type(),
                  lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
        EXPECT_FALSE(inject_input.is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
        unmount_run_loop.Quit();
      });

  child_handler->RemoveLensCrop();
  unmount_run_loop.Run();

  EXPECT_FALSE(model1->GetLensCrop().has_value());
  EXPECT_FALSE(model1->lens_crop().has_value());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_SearchToClientMessageHandshakeResponse) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, OnHandshakeComplete())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  EXPECT_CALL(*mock_controller_, SetAuthUserIndex(2));

  lens::SearchToClientMessage message;
  message.mutable_handshake_response()->set_auth_user_index(2);
  const size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  message.SerializeToArray(serialized_message.data(), size);

  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();

  EXPECT_EQ(mock_session_handle_->auth_user_index(), 2u);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       OnWebviewMessage_AimToClientMessageHandshakeResponse) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, OnHandshakeComplete())
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  lens::AimToClientMessage message;
  message.mutable_handshake_response();
  const size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  message.SerializeToArray(serialized_message.data(), size);

  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       MultipleFramesDeduplicateSearchMessages) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  handler_->SetTaskId(task_id);

  // Primary handler (handler_) already has mock_page_ bound in
  // SetUpOnMainThread. Create a child frame with a second handler and second
  // bound mock page.
  ASSERT_TRUE(
      content::ExecJs(web_contents_,
                      "const iframe = document.createElement('iframe'); "
                      "document.body.appendChild(iframe);"));
  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(web_contents_->GetPrimaryMainFrame(), 0);
  ASSERT_NE(child_rfh, nullptr);

  ContextualTasksExtensionHandler::CreateForCurrentDocument(child_rfh);
  auto* child_handler =
      ContextualTasksExtensionHandler::GetForCurrentDocument(child_rfh);
  ASSERT_NE(child_handler, nullptr);

  NiceMock<MockContextualTasksExtensionPage> mock_child_page;
  mojo::PendingReceiver<mojom::ExtensionPageHandler>
      child_page_handler_receiver;
  child_handler->CreateExtensionPageHandler(
      mock_child_page.BindAndGetRemote(),
      std::move(child_page_handler_receiver));

  // Verify that only the primary handler (handler_) emits the mount message,
  // and the child handler does not emit a duplicate message.
  base::RunLoop mount_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        EXPECT_TRUE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        mount_run_loop.Quit();
      });
  EXPECT_CALL(mock_child_page, PostSearchMessage(_)).Times(0);

  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,test_crop");
  mount_run_loop.Run();

  // Next, verify that when child_handler removes the crop, only the primary
  // handler emits the unmount message.
  auto model = handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model && model->lens_crop().has_value());

  base::RunLoop unmount_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        EXPECT_FALSE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
        unmount_run_loop.Quit();
      });
  EXPECT_CALL(mock_child_page, PostSearchMessage(_)).Times(0);

  child_handler->RemoveLensCrop();
  unmount_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       OnLensThumbnailCreated_EmitsInjectChromeInput) {
  base::RunLoop run_loop;

  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        const auto& inject_input = message->inject_chrome_input();
        EXPECT_EQ(inject_input.input_type(),
                  lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
        EXPECT_TRUE(inject_input.is_active());

        auto model = handler_->GetOrCreateInputStateModelForTesting();
        ASSERT_TRUE(model && model->lens_crop().has_value());
        EXPECT_EQ("data:image/png;base64,test_crop",
                  model->lens_crop()->data_uri);
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        run_loop.Quit();
      });

  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,test_crop");
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_OnSubmitQueryRequest_WithoutUploadedContext_ReturnsEmptyResponse) {
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents_);
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());

  NiceMock<MockLensOverlayController> mock_overlay(tab, mock_lens_controller_,
                                                   profile->GetPrefs());
  NiceMock<lens::MockLensOverlayQueryController> mock_query_controller(nullptr);

  EXPECT_CALL(*mock_lens_controller_, lens_overlay_controller())
      .WillRepeatedly(Return(&mock_overlay));
  EXPECT_CALL(*mock_lens_controller_, lens_overlay_query_controller())
      .WillRepeatedly(Return(&mock_query_controller));
  EXPECT_CALL(*mock_lens_controller_, IsCurrentTabSameOrigin())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_overlay, HasRegionSelection()).WillRepeatedly(Return(true));

  // No uploaded files in session handle.
  EXPECT_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillRepeatedly(Return(std::vector<contextual_search::FileInfo>{}));

  auto model = handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model);
  model->SetLensCrop("data:image/png;base64,test_crop");
  mock_page_.FlushForTesting();
  EXPECT_TRUE(model->lens_crop().has_value());

  bool received_submit_response = false;
  bool received_lens_chip_unmount = false;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillRepeatedly([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        if (message->has_on_submit_query_response()) {
          const auto& response = message->on_submit_query_response();
          EXPECT_EQ(0, response.added_contexts_size());
          received_submit_response = true;
        } else if (message->has_inject_chrome_input()) {
          const auto& inject_input = message->inject_chrome_input();
          EXPECT_EQ(inject_input.input_type(),
                    lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
          EXPECT_FALSE(inject_input.is_active());
          received_lens_chip_unmount = true;
        }
        if (received_submit_response && received_lens_chip_unmount) {
          run_loop.Quit();
        }
      });

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();

  EXPECT_FALSE(model->lens_crop().has_value());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_OnSubmitQueryRequest_WithSessionHandle_ReturnsMatchingAddedContext) {
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents_);
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());

  NiceMock<MockLensOverlayController> mock_overlay(tab, mock_lens_controller_,
                                                   profile->GetPrefs());
  NiceMock<lens::MockLensOverlayQueryController> mock_query_controller(nullptr);

  EXPECT_CALL(*mock_lens_controller_, lens_overlay_controller())
      .WillRepeatedly(Return(&mock_overlay));
  EXPECT_CALL(*mock_lens_controller_, lens_overlay_query_controller())
      .WillRepeatedly(Return(&mock_query_controller));
  EXPECT_CALL(*mock_lens_controller_, IsCurrentTabSameOrigin())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_overlay, HasRegionSelection()).WillRepeatedly(Return(true));

  // Set up mock session handle values.
  EXPECT_CALL(*mock_session_handle_, search_session_id())
      .WillRepeatedly(Return("session_sticky_id"));

  base::UnguessableToken overlay_token = base::UnguessableToken::Create();
  contextual_search::FileInfo file_info;
  file_info.file_token = overlay_token;
  file_info.is_implicit_upload = true;
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(999);
  req_id.set_context_id(888);
  req_id.set_image_sequence_id(1);
  file_info.request_id = req_id;
  file_info.input_data = std::make_unique<lens::ContextualInputData>();
  file_info.input_data->upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY;

  std::vector<contextual_search::FileInfo> file_infos = {file_info};
  EXPECT_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillRepeatedly(Return(file_infos));

  lens::LensOverlayVisualSearchInteractionData mock_vsint;
  mock_vsint.set_interaction_type(
      lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  mock_vsint.mutable_zoomed_crop()->mutable_crop()->set_center_x(0.35f);
  EXPECT_CALL(*mock_session_handle_, GetVisualSearchInteractionData(_, _))
      .WillOnce(Return(std::make_optional(mock_vsint)));

  auto model = handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model);
  model->SetLensCrop("data:image/png;base64,test_crop");
  mock_page_.FlushForTesting();
  EXPECT_TRUE(model->lens_crop().has_value());

  EXPECT_CALL(mock_overlay, ClearRegionSelection()).Times(0);
  EXPECT_CALL(
      *mock_lens_controller_,
      CloseLensAsync(
          lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted))
      .Times(1);

  bool received_submit_response = false;
  bool received_lens_chip_unmount = false;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillRepeatedly([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        if (message->has_on_submit_query_response()) {
          const auto& response = message->on_submit_query_response();
          ASSERT_EQ(1, response.added_contexts_size());
          const auto& added = response.added_contexts(0);
          EXPECT_EQ("session_sticky_id", added.search_session_id());
          EXPECT_EQ(999u, added.request_id().uuid());
          EXPECT_EQ(888, added.request_id().context_id());
          EXPECT_EQ(1, added.request_id().image_sequence_id());
          EXPECT_EQ(lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE,
                    added.request_id().media_type());
          EXPECT_TRUE(added.has_visual_search_interaction_data());
          EXPECT_EQ(lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH,
                    added.visual_search_interaction_data().interaction_type());
          EXPECT_EQ(
              lens::LensOverlayContextualInputUploadType::
                  CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY,
              added.contextual_input_upload_type());
          received_submit_response = true;
        } else if (message->has_inject_chrome_input()) {
          const auto& inject_input = message->inject_chrome_input();
          EXPECT_EQ(inject_input.input_type(),
                    lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
          EXPECT_FALSE(inject_input.is_active());
          received_lens_chip_unmount = true;
        }
        if (received_submit_response && received_lens_chip_unmount) {
          run_loop.Quit();
        }
      });

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();

  EXPECT_FALSE(model->lens_crop().has_value());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_OnSubmitQueryRequest_WithSubmittedContext_ReturnsMatchingAddedContext) {
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents_);
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());

  NiceMock<MockLensOverlayController> mock_overlay(tab, mock_lens_controller_,
                                                   profile->GetPrefs());
  NiceMock<lens::MockLensOverlayQueryController> mock_query_controller(nullptr);

  EXPECT_CALL(*mock_lens_controller_, lens_overlay_controller())
      .WillRepeatedly(Return(&mock_overlay));
  EXPECT_CALL(*mock_lens_controller_, lens_overlay_query_controller())
      .WillRepeatedly(Return(&mock_query_controller));
  EXPECT_CALL(*mock_lens_controller_, IsCurrentTabSameOrigin())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_overlay, HasRegionSelection()).WillRepeatedly(Return(true));

  EXPECT_CALL(*mock_session_handle_, search_session_id())
      .WillRepeatedly(Return("session_sticky_id"));

  base::UnguessableToken overlay_token = base::UnguessableToken::Create();
  contextual_search::FileInfo file_info;
  file_info.file_token = overlay_token;
  file_info.is_implicit_upload = true;
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(999);
  req_id.set_context_id(888);
  req_id.set_image_sequence_id(1);
  file_info.request_id = req_id;
  file_info.input_data = std::make_unique<lens::ContextualInputData>();
  file_info.input_data->upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY;

  std::vector<contextual_search::FileInfo> file_infos = {file_info};
  EXPECT_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillRepeatedly(Return(std::vector<contextual_search::FileInfo>{}));
  EXPECT_CALL(*mock_session_handle_, GetSubmittedContextFileInfos())
      .WillRepeatedly(Return(file_infos));

  lens::LensOverlayVisualSearchInteractionData mock_vsint;
  mock_vsint.set_interaction_type(
      lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  mock_vsint.mutable_zoomed_crop()->mutable_crop()->set_center_x(0.35f);
  EXPECT_CALL(*mock_session_handle_, GetVisualSearchInteractionData(_, _))
      .WillOnce(Return(std::make_optional(mock_vsint)));

  handler_->GetOrCreateInputStateModelForTesting()->SetLensCrop(
      "data:image/png;base64,test_crop");

  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillRepeatedly([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        if (!message.has_value() || !message->has_on_submit_query_response()) {
          return;
        }
        const auto& response = message->on_submit_query_response();
        ASSERT_EQ(1, response.added_contexts_size());
        const auto& added = response.added_contexts(0);
        EXPECT_EQ("session_sticky_id", added.search_session_id());
        EXPECT_EQ(999u, added.request_id().uuid());
        EXPECT_EQ(888, added.request_id().context_id());
        EXPECT_EQ(1, added.request_id().image_sequence_id());
        EXPECT_EQ(lens::LensOverlayRequestId::MEDIA_TYPE_DEFAULT_IMAGE,
                  added.request_id().media_type());
        EXPECT_TRUE(added.has_visual_search_interaction_data());
        EXPECT_EQ(lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH,
                  added.visual_search_interaction_data().interaction_type());
        EXPECT_EQ(
            lens::LensOverlayContextualInputUploadType::
                CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY,
            added.contextual_input_upload_type());
        run_loop.Quit();
      });

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_OnSubmitQueryRequest_FallbackToOverlayScreenshotAndRegion) {
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents_);
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());

  NiceMock<MockLensOverlayController> mock_overlay(tab, mock_lens_controller_,
                                                   profile->GetPrefs());
  NiceMock<lens::MockLensOverlayQueryController> mock_query_controller(nullptr);

  EXPECT_CALL(*mock_lens_controller_, lens_overlay_controller())
      .WillRepeatedly(Return(&mock_overlay));
  EXPECT_CALL(*mock_lens_controller_, lens_overlay_query_controller())
      .WillRepeatedly(Return(&mock_query_controller));
  EXPECT_CALL(*mock_lens_controller_, IsCurrentTabSameOrigin())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_overlay, HasRegionSelection()).WillRepeatedly(Return(true));

  base::UnguessableToken overlay_token = base::UnguessableToken::Create();
  contextual_search::FileInfo file_info;
  file_info.file_token = overlay_token;
  file_info.is_implicit_upload = true;
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(42);
  file_info.request_id = req_id;
  file_info.input_data = std::make_unique<lens::ContextualInputData>();
  file_info.input_data->upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY;

  std::vector<contextual_search::FileInfo> file_infos = {file_info};
  EXPECT_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillRepeatedly(Return(file_infos));
  EXPECT_CALL(*mock_session_handle_, GetVisualSearchInteractionData(_, _))
      .WillOnce(Return(std::nullopt));

  // Neither session handle nor query controller returns visual search
  // interaction data, triggering the fallback path that reads from
  // overlay->selected_region() and overlay->initial_screenshot().
  EXPECT_CALL(mock_query_controller, GetVisualSearchInteractionData())
      .WillOnce(Return(std::nullopt));

  mock_overlay.selected_region_ = lens::mojom::CenterRotatedBox::New();
  mock_overlay.selected_region_->box = gfx::RectF(0.2f, 0.3f, 0.4f, 0.5f);
  mock_overlay.selected_region_->coordinate_type =
      lens::mojom::CenterRotatedBox_CoordinateType::kNormalized;

  SkBitmap screenshot;
  screenshot.allocN32Pixels(200, 100);
  screenshot.eraseColor(SK_ColorRED);
  mock_overlay.initial_screenshot_ = screenshot;

  handler_->GetOrCreateInputStateModelForTesting()->SetLensCrop(
      "data:image/png;base64,test_crop");

  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillRepeatedly([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        if (!message.has_value() || !message->has_on_submit_query_response()) {
          return;
        }
        const auto& response = message->on_submit_query_response();
        ASSERT_EQ(1, response.added_contexts_size());
        const auto& added = response.added_contexts(0);
        EXPECT_EQ(42u, added.request_id().uuid());
        EXPECT_TRUE(added.has_visual_search_interaction_data());
        const auto& vsint = added.visual_search_interaction_data();
        EXPECT_EQ(lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH,
                  vsint.interaction_type());
        EXPECT_TRUE(vsint.has_zoomed_crop());
        EXPECT_EQ(100, vsint.zoomed_crop().parent_height());
        EXPECT_EQ(200, vsint.zoomed_crop().parent_width());
        EXPECT_FLOAT_EQ(0.2f, vsint.zoomed_crop().crop().center_x());
        EXPECT_FLOAT_EQ(0.3f, vsint.zoomed_crop().crop().center_y());
        EXPECT_FLOAT_EQ(0.4f, vsint.zoomed_crop().crop().width());
        EXPECT_FLOAT_EQ(0.5f, vsint.zoomed_crop().crop().height());
        run_loop.Quit();
      });

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_OnSubmitQueryRequest_WithoutRegion_ReturnsEmptyResponse) {
  EXPECT_CALL(
      *mock_lens_controller_,
      CloseLensAsync(
          lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted))
      .Times(1);

  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_on_submit_query_response());
        EXPECT_EQ(0, message->on_submit_query_response().added_contexts_size());
        run_loop.Quit();
      });

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_OnSubmitQueryRequest_WithoutRecentInteraction_DoesNotRespond) {
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents_);
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());

  NiceMock<MockLensOverlayController> mock_overlay(tab, mock_lens_controller_,
                                                   profile->GetPrefs());
  NiceMock<lens::MockLensOverlayQueryController> mock_query_controller(nullptr);

  EXPECT_CALL(*mock_lens_controller_, lens_overlay_controller())
      .WillRepeatedly(Return(&mock_overlay));
  EXPECT_CALL(*mock_lens_controller_, lens_overlay_query_controller())
      .WillRepeatedly(Return(&mock_query_controller));
  EXPECT_CALL(*mock_lens_controller_, IsCurrentTabSameOrigin())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_overlay, HasRegionSelection()).WillRepeatedly(Return(true));

  base::UnguessableToken overlay_token = base::UnguessableToken::Create();
  contextual_search::FileInfo file_info;
  file_info.file_token = overlay_token;
  file_info.is_implicit_upload = true;
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(42);
  file_info.request_id = req_id;
  file_info.input_data = std::make_unique<lens::ContextualInputData>();
  file_info.input_data->upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY;

  std::vector<contextual_search::FileInfo> file_infos = {file_info};
  EXPECT_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillRepeatedly(Return(file_infos));

  handler_->GetOrCreateInputStateModelForTesting()->SetLensCrop(
      "data:image/png;base64,test_crop");

  // Ensure no user interaction was simulated on web_contents_.
  ASSERT_FALSE(web_contents_->HasRecentInteraction());

  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillRepeatedly([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        if (message.has_value()) {
          EXPECT_FALSE(message->has_on_submit_query_response());
        }
      });

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  handler_->OnWebviewMessage(serialized_message);
  mock_page_.FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_OnSubmitQueryRequest_ReplayWithoutNewInteraction_DoesNotRespond) {
  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  tabs::TabInterface* tab = tabs::TabInterface::GetFromContents(web_contents_);
  Profile* profile = Profile::FromBrowserContext(rfh->GetBrowserContext());

  NiceMock<MockLensOverlayController> mock_overlay(tab, mock_lens_controller_,
                                                   profile->GetPrefs());
  NiceMock<lens::MockLensOverlayQueryController> mock_query_controller(nullptr);

  EXPECT_CALL(*mock_lens_controller_, lens_overlay_controller())
      .WillRepeatedly(Return(&mock_overlay));
  EXPECT_CALL(*mock_lens_controller_, lens_overlay_query_controller())
      .WillRepeatedly(Return(&mock_query_controller));
  EXPECT_CALL(*mock_lens_controller_, IsCurrentTabSameOrigin())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(mock_overlay, HasRegionSelection()).WillRepeatedly(Return(true));

  base::UnguessableToken overlay_token = base::UnguessableToken::Create();
  contextual_search::FileInfo file_info;
  file_info.file_token = overlay_token;
  file_info.is_implicit_upload = true;
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(42);
  file_info.request_id = req_id;
  file_info.input_data = std::make_unique<lens::ContextualInputData>();
  file_info.input_data->upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_CONTEXTUAL_SEARCHBOX_INITIAL_QUERY;

  std::vector<contextual_search::FileInfo> file_infos = {file_info};
  EXPECT_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillRepeatedly(Return(file_infos));

  lens::LensOverlayVisualSearchInteractionData mock_vsint;
  mock_vsint.set_interaction_type(
      lens::LensOverlayInteractionRequestMetadata::REGION_SEARCH);
  mock_vsint.mutable_zoomed_crop()->mutable_crop()->set_center_x(0.35f);
  EXPECT_CALL(*mock_session_handle_, GetVisualSearchInteractionData(_, _))
      .WillRepeatedly(Return(std::make_optional(mock_vsint)));

  auto model = handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model);
  model->SetLensCrop("data:image/png;base64,test_crop");
  mock_page_.FlushForTesting();

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  // 1. Simulate user interaction for the first submission.
  SimulateUserInteraction();

  base::RunLoop run_loop1;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillRepeatedly([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        if (!message.has_value() || !message->has_on_submit_query_response()) {
          return;
        }
        EXPECT_EQ(1, message->on_submit_query_response().added_contexts_size());
        run_loop1.Quit();
      });

  handler_->OnWebviewMessage(serialized_message);
  run_loop1.Run();
  mock_page_.FlushForTesting();
  EXPECT_FALSE(model->lens_crop().has_value());

  // Set a new crop so that if the replay were processed, it would attach a
  // context and clear the crop.
  model->SetLensCrop("data:image/png;base64,test_crop_2");
  mock_page_.FlushForTesting();

  // 2. Replay the same OnSubmitQueryRequest WITHOUT a new user interaction.
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillRepeatedly([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        if (message.has_value()) {
          EXPECT_FALSE(message->has_on_submit_query_response());
        }
      });
  handler_->OnWebviewMessage(serialized_message);
  mock_page_.FlushForTesting();
  EXPECT_TRUE(model->lens_crop().has_value());

  // 3. Simulate a new user interaction and verify submission succeeds again.
  SimulateUserInteraction();

  base::RunLoop run_loop2;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillRepeatedly([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        if (!message.has_value() || !message->has_on_submit_query_response()) {
          return;
        }
        EXPECT_EQ(1, message->on_submit_query_response().added_contexts_size());
        run_loop2.Quit();
      });

  handler_->OnWebviewMessage(serialized_message);
  run_loop2.Run();
  EXPECT_FALSE(model->lens_crop().has_value());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       RemoveLensCrop_DismissesLensClearsModelAndEmitsUnmount) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        const auto& inject_input = message->inject_chrome_input();
        EXPECT_EQ(inject_input.input_type(),
                  lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
        EXPECT_TRUE(inject_input.is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        run_loop.Quit();
      });
  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,test_crop");
  run_loop.Run();

  auto model = handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model);
  EXPECT_TRUE(model->GetLensCrop().has_value());

  // Verify that ClearRegionSelection() (via lens_overlay_controller()) is
  // called strictly before CloseLensAsync() (load-bearing ordering 1-before-2).
  EXPECT_CALL(*mock_lens_controller_, lens_overlay_controller())
      .WillRepeatedly(Return(mock_lens_overlay_controller_.get()));
  {
    testing::InSequence seq;
    EXPECT_CALL(*mock_lens_overlay_controller_, ClearRegionSelection())
        .Times(1);
    EXPECT_CALL(
        *mock_lens_controller_,
        CloseLensAsync(
            lens::LensOverlayDismissalSource::kContextualTasksLensChipRemoved))
        .Times(1);
  }

  base::RunLoop run_loop2;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        const auto& inject_input = message->inject_chrome_input();
        EXPECT_EQ(inject_input.input_type(),
                  lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
        EXPECT_FALSE(inject_input.is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
        run_loop2.Quit();
      });

  handler_->RemoveLensCrop();
  run_loop2.Run();

  EXPECT_FALSE(model->GetLensCrop().has_value());
  EXPECT_FALSE(model->lens_crop().has_value());
  EXPECT_EQ(std::nullopt, handler_->GetLensOverlayTokenForTesting());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       RemoveLensCrop_WhenNoCropDoesNotDismiss) {
  auto model = handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model);
  EXPECT_FALSE(model->lens_crop().has_value());

  // Calling RemoveLensCrop when no crop exists should be a no-op:
  // no CloseLensAsync, no ClearRegionSelection, no PostSearchMessage.
  EXPECT_CALL(*mock_lens_controller_, CloseLensAsync(_)).Times(0);
  EXPECT_CALL(mock_page_, PostSearchMessage(_)).Times(0);

  handler_->RemoveLensCrop();

  EXPECT_FALSE(model->lens_crop().has_value());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnLensOverlayStateChanged_PreservesLensCropWhenOverlayClosed) {
  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        const auto& inject_input = message->inject_chrome_input();
        EXPECT_EQ(inject_input.input_type(),
                  lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
        EXPECT_TRUE(inject_input.is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        run_loop.Quit();
      });
  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,test_crop");
  run_loop.Run();

  auto model = handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model);
  EXPECT_TRUE(model->GetLensCrop().has_value());

  EXPECT_CALL(mock_page_, PostSearchMessage(_)).Times(0);
  EXPECT_CALL(mock_page_, OnLensOverlayStateChanged(false)).Times(1);

  handler_->OnLensOverlayStateChanged(false);
  mock_page_.FlushForTesting();

  EXPECT_TRUE(model->GetLensCrop().has_value());
  EXPECT_TRUE(model->lens_crop().has_value());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       SuccessiveCropReplacedInPlaceWithoutUnmount) {
  // Step 1: Emit first crop. Should emit mount (is_active=true) and dispatch
  // OnLensCropUpdated to the page.
  base::RunLoop run_loop1;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        EXPECT_TRUE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        run_loop1.Quit();
      });
  EXPECT_CALL(mock_page_,
              OnLensCropUpdated(GURL("data:image/png;base64,crop1")))
      .Times(1);

  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,crop1");
  run_loop1.Run();

  // Step 2: Emit second crop immediately without prior dismissal.
  // Must update the chip in-place via OnLensCropUpdated without emitting any
  // new PostSearchMessage (mount or unmount) to AIM.
  base::RunLoop run_loop2;
  EXPECT_CALL(mock_page_, PostSearchMessage(_)).Times(0);
  EXPECT_CALL(mock_page_,
              OnLensCropUpdated(GURL("data:image/png;base64,crop2")))
      .WillOnce([&](const GURL& data_uri) {
        EXPECT_EQ("data:image/png;base64,crop2", data_uri.spec());
        run_loop2.Quit();
      });

  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,crop2");
  run_loop2.Run();

  // Step 3: Dismiss second crop. Should now emit unmount (is_active=false).
  EXPECT_CALL(*mock_lens_controller_, lens_overlay_controller())
      .WillRepeatedly(Return(mock_lens_overlay_controller_.get()));
  EXPECT_CALL(*mock_lens_overlay_controller_, ClearRegionSelection()).Times(1);
  EXPECT_CALL(
      *mock_lens_controller_,
      CloseLensAsync(
          lens::LensOverlayDismissalSource::kContextualTasksLensChipRemoved))
      .Times(1);

  base::RunLoop run_loop3;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        EXPECT_FALSE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
        run_loop3.Quit();
      });

  handler_->RemoveLensCrop();
  run_loop3.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    CreateExtensionPageHandler_InitializesInputStateModelAndEmitsPostSearchMessage) {
  if (auto* user_data =
          contextual_tasks::ContextualTasksWebContentsUserData::FromWebContents(
              web_contents_)) {
    user_data->UnregisterExtensionFrame(handler_);
  }
  ASSERT_TRUE(
      content::ExecJs(web_contents_,
                      "const iframe = document.createElement('iframe'); "
                      "document.body.appendChild(iframe);"));
  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(web_contents_->GetPrimaryMainFrame(), 0);
  ASSERT_NE(child_rfh, nullptr);

  ContextualTasksExtensionHandler::CreateForCurrentDocument(child_rfh);
  auto* child_handler =
      ContextualTasksExtensionHandler::GetForCurrentDocument(child_rfh);
  ASSERT_NE(child_handler, nullptr);

  testing::NiceMock<MockContextualTasksExtensionPage> mock_child_page;
  mojo::PendingReceiver<mojom::ExtensionPageHandler> child_handler_receiver;
  child_handler->CreateExtensionPageHandler(mock_child_page.BindAndGetRemote(),
                                            std::move(child_handler_receiver));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_child_page, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        const auto& inject_input = message->inject_chrome_input();
        EXPECT_EQ(inject_input.input_type(),
                  lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
        EXPECT_TRUE(inject_input.is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        run_loop.Quit();
      });

  child_handler->OnLensThumbnailCreatedForTesting(
      "data:image/png;base64,test_crop");
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnInputStateChanged_LensChipUrlDoesNotEmitPostSearchMessage) {
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.relative_url.find("lens_chip.html") != std::string::npos) {
          auto response =
              std::make_unique<net::test_server::BasicHttpResponse>();
          response->set_code(net::HTTP_OK);
          response->set_content_type("text/html");
          response->set_content("<html><body>lens chip</body></html>");
          return response;
        }
        return nullptr;
      }));
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), embedded_test_server()->GetURL("/title1.html"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  content::WebContents* test_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  GURL chip_url = embedded_test_server()->GetURL("/lens_chip.html");
  ASSERT_TRUE(
      content::ExecJs(test_contents,
                      "const iframe = document.createElement('iframe'); "
                      "iframe.id = 'chip_iframe'; "
                      "iframe.name = 'chip_iframe'; "
                      "document.body.appendChild(iframe);"));
  ASSERT_TRUE(
      content::NavigateIframeToURL(test_contents, "chip_iframe", chip_url));

  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(test_contents->GetPrimaryMainFrame(), 0);
  ASSERT_NE(child_rfh, nullptr);
  ContextualTasksExtensionHandler::CreateForCurrentDocument(child_rfh);
  auto* chip_handler =
      ContextualTasksExtensionHandler::GetForCurrentDocument(child_rfh);
  ASSERT_NE(chip_handler, nullptr);

  testing::NiceMock<MockContextualTasksExtensionPage> mock_chip_page;
  mojo::PendingReceiver<mojom::ExtensionPageHandler> chip_handler_receiver;
  chip_handler->CreateExtensionPageHandler(mock_chip_page.BindAndGetRemote(),
                                           std::move(chip_handler_receiver));

  EXPECT_CALL(mock_chip_page, PostSearchMessage(_)).Times(0);

  chip_handler->OnLensThumbnailCreatedForTesting(
      "data:image/png;base64,test_crop");
  base::RunLoop run_loop;
  base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, run_loop.QuitClosure());
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    PostSearchMessage_FakeAimPageReceivesProtoWithTargetOrigin) {
  base::RunLoop run_loop;
  std::string serialized_proto_bytes;

  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        const auto& inject_input = message->inject_chrome_input();
        EXPECT_EQ(inject_input.input_type(),
                  lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
        EXPECT_TRUE(inject_input.is_active());
        EXPECT_TRUE(message->SerializeToString(&serialized_proto_bytes));
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        run_loop.Quit();
      });

  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,test_crop");
  run_loop.Run();

  ASSERT_FALSE(serialized_proto_bytes.empty());

  lens::ClientToSearchMessage parsed_message;
  ASSERT_TRUE(parsed_message.ParseFromString(serialized_proto_bytes));
  EXPECT_TRUE(parsed_message.has_inject_chrome_input());
  EXPECT_EQ(parsed_message.inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
  EXPECT_TRUE(parsed_message.inject_chrome_input().is_active());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       AddTabContext_NullPageContentData) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("about:blank"), WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* blank_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(blank_tab, nullptr);
  int32_t blank_tab_id = blank_tab->GetHandle().raw_value();

  EXPECT_CALL(*mock_session_handle_,
              StartTabContextUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  base::RunLoop run_loop;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      blank_tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            EXPECT_TRUE(result.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  base::RunLoop apc_loop;
  lens::TabContextualizationController::From(blank_tab)->GetPageContext(
      base::BindLambdaForTesting(
          [&](std::unique_ptr<lens::ContextualInputData> data) {
            EXPECT_EQ(data, nullptr);
            apc_loop.Quit();
          }));
  apc_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       InjectChromeInput_UpdatesOnTabSelection) {
  base::RunLoop run_loop_add;
  EXPECT_CALL(mock_page_, PostSearchMessage(testing::_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto client_message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(client_message.has_value());
        ASSERT_TRUE(client_message->has_inject_chrome_input());
        EXPECT_EQ(
            client_message->inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
        EXPECT_TRUE(client_message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto client_message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(client_message.has_value());
        ASSERT_TRUE(client_message->has_on_context_state_changed());
        EXPECT_EQ(client_message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_UPLOADING);
        run_loop_add.Quit();
      });

  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t tab_id = active_tab->GetHandle().raw_value();

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu, base::DoNothing());
  run_loop_add.Run();

  base::RunLoop run_loop_clear;
  EXPECT_CALL(mock_page_, PostSearchMessage(testing::_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto client_message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(client_message.has_value());
        ASSERT_TRUE(client_message->has_inject_chrome_input());
        EXPECT_EQ(
            client_message->inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
        EXPECT_FALSE(client_message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto client_message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(client_message.has_value());
        ASSERT_TRUE(client_message->has_on_context_state_changed());
        EXPECT_EQ(client_message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
        run_loop_clear.Quit();
      });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->ClearFiles(
      /*should_block_auto_suggested_tabs=*/false);
  run_loop_clear.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       OnWebviewMessage_OnSubmitQueryRequest) {
  contextual_search::FileInfo file_info;
  file_info.file_token = base::UnguessableToken::Create();
  AttachActiveTab(file_info.file_token);
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(999);
  file_info.request_id = req_id;
  file_info.input_data = std::make_unique<lens::ContextualInputData>();
  file_info.input_data->upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;

  ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(
          Return(std::vector<contextual_search::FileInfo>{file_info}));
  ON_CALL(*mock_session_handle_, search_session_id())
      .WillByDefault(Return("test_session_id"));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(testing::_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto client_message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(client_message.has_value());
        ASSERT_TRUE(client_message->has_on_submit_query_response());
        const auto& response = client_message->on_submit_query_response();
        ASSERT_EQ(response.added_contexts_size(), 1);
        EXPECT_EQ(response.added_contexts(0).search_session_id(),
                  "test_session_id");
        EXPECT_EQ(response.added_contexts(0).request_id().context_id(), 999);
        EXPECT_EQ(response.added_contexts(0).contextual_input_upload_type(),
                  lens::LensOverlayContextualInputUploadType::
                      CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT);
        run_loop.Quit();
      });

  lens::SearchToClientMessage search_message;
  search_message.mutable_on_submit_query_request();
  const size_t size = search_message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  search_message.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       AddTabContext_AttachesTabInSessionHandle) {
  tabs::TabInterface* first_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(first_tab, nullptr);
  int32_t first_tab_id = first_tab->GetHandle().raw_value();
  SessionID first_session_id =
      sessions::SessionTabHelper::IdForTab(first_tab->GetContents());

  base::RunLoop first_run_loop;
  base::UnguessableToken first_token;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      first_tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            first_token = result.value();
            first_run_loop.Quit();
          }));
  first_run_loop.Run();

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com/second_tab"),
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* second_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(second_tab, nullptr);
  int32_t second_tab_id = second_tab->GetHandle().raw_value();
  SessionID second_session_id =
      sessions::SessionTabHelper::IdForTab(second_tab->GetContents());

  base::RunLoop second_run_loop;
  base::UnguessableToken second_token;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      second_tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            second_token = result.value();
            second_run_loop.Quit();
          }));
  second_run_loop.Run();

  const auto& attached = mock_session_handle_->GetTabContextState().attached;
  ASSERT_EQ(attached.size(), 2u);
  EXPECT_EQ(attached[0].context_token, first_token);
  EXPECT_EQ(attached[0].tab_id, first_session_id.id());
  EXPECT_EQ(attached[1].context_token, second_token);
  EXPECT_EQ(attached[1].tab_id, second_session_id.id());

  // Deleting the second tab should remove only the second tab's token even
  // when second_tab_id (TabHandle) equals first_session_id.id() (SessionID).
  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      second_tab_id);
  ASSERT_EQ(mock_session_handle_->GetTabContextState().attached.size(), 1u);
  EXPECT_EQ(
      mock_session_handle_->GetTabContextState().attached[0].context_token,
      first_token);

  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      first_tab_id);
  EXPECT_TRUE(mock_session_handle_->GetTabContextState().attached.empty());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       OnWebviewMessage_OnSubmitQueryRequestSkipsSubmittedTab) {
  contextual_search::FileInfo file_info;
  file_info.file_token = base::UnguessableToken::Create();
  // Attach the tab and then clear `uploaded_context_tokens_`, simulating a tab
  // from a previous turn that remains in `tab_context_.attached` after
  // `ClearFiles(query_submitted=true)`.
  AttachActiveTab(file_info.file_token);
  ASSERT_EQ(mock_session_handle_->GetTabContextState().attached.size(), 1u);
  mock_session_handle_->GetUploadedContextTokensForTesting().clear();

  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(999);
  file_info.request_id = req_id;
  ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(
          Return(std::vector<contextual_search::FileInfo>{file_info}));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(testing::_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto client_message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(client_message.has_value());
        ASSERT_TRUE(client_message->has_on_submit_query_response());
        EXPECT_EQ(
            client_message->on_submit_query_response().added_contexts_size(),
            0);
        run_loop.Quit();
      });

  lens::SearchToClientMessage search_message;
  search_message.mutable_on_submit_query_request();
  const size_t size = search_message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  search_message.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();
}

// Deselection of previously submitted tabs is tracked only with context
// management enabled.
class ContextualTasksExtensionHandlerContextManagementBrowserTest
    : public ContextualTasksExtensionHandlerBrowserTestBase {
 public:
  ContextualTasksExtensionHandlerContextManagementBrowserTest()
      : ContextualTasksExtensionHandlerBrowserTestBase(
            {kContextualTasks, kContextualTasksRearchitecture,
             kContextualTasksForceEntryPointEligibility,
             omnibox::kContextManagementInComposebox},
            {}) {}
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextManagementBrowserTest,
    OnWebviewMessage_OnSubmitQueryRequest_ReportsRemovedTabAndMarksSubmitted) {
  contextual_search::FileInfo file_info;
  file_info.file_token = base::UnguessableToken::Create();
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(4242);
  file_info.request_id = req_id;
  AttachActiveTab(file_info.file_token);
  ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(
          Return(std::vector<contextual_search::FileInfo>{file_info}));

  // A tab submitted in an earlier turn that the user has since deselected is
  // pending removal on the server.
  const SessionID deselected_session_id = SessionID::FromSerializedValue(777);
  lens::LensOverlayRequestId removed_req_id;
  removed_req_id.set_uuid(777);
  mock_session_handle_->set_persisted_tabs(
      {{deselected_session_id,
        {base::UnguessableToken::Create(), removed_req_id}}});
  mock_session_handle_->set_deselected_tabs_urls(
      {{deselected_session_id, {GURL("https://example.com/old"), "Old"}}});

  auto submit =
      [&](base::OnceCallback<void(
              const lens::ClientToSearchMessage::OnSubmitQueryResponse&)>
              on_response) {
        base::RunLoop run_loop;
        EXPECT_CALL(mock_page_, PostSearchMessage(_))
            .WillRepeatedly([&](mojo_base::ProtoWrapper wrapper) {
              auto message = wrapper.As<lens::ClientToSearchMessage>();
              if (!message.has_value() ||
                  !message->has_on_submit_query_response() || !on_response) {
                return;
              }
              std::move(on_response).Run(message->on_submit_query_response());
              run_loop.Quit();
            });
        lens::SearchToClientMessage request;
        request.mutable_on_submit_query_request();
        const size_t size = request.ByteSizeLong();
        std::vector<uint8_t> serialized_message(size);
        request.SerializeToArray(serialized_message.data(), size);
        SimulateUserInteraction();
        handler_->OnWebviewMessage(serialized_message);
        run_loop.Run();
        testing::Mock::VerifyAndClearExpectations(&mock_page_);
      };

  // First turn: the uploaded tab is added and the deselected tab is reported
  // as removed.
  submit(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage::OnSubmitQueryResponse& response) {
        ASSERT_EQ(1, response.added_contexts_size());
        EXPECT_EQ(4242u, response.added_contexts(0).request_id().uuid());
        ASSERT_EQ(1, response.removed_contexts_size());
        EXPECT_EQ(777u, response.removed_contexts(0).request_id().uuid());
      }));

  // Submission moves the uploaded token into the submitted set and persisted
  // tabs, and drops the reported tab so its removal is only sent once.
  EXPECT_TRUE(mock_session_handle_->GetUploadedContextTokens().empty());
  EXPECT_THAT(mock_session_handle_->GetSubmittedContextTokens(),
              testing::ElementsAre(file_info.file_token));
  EXPECT_TRUE(mock_session_handle_->has_submitted_context());
  EXPECT_FALSE(
      mock_session_handle_->persisted_tabs().contains(deselected_session_id));

  // Second turn: nothing new was uploaded or removed.
  submit(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage::OnSubmitQueryResponse& response) {
        EXPECT_EQ(0, response.added_contexts_size());
        EXPECT_EQ(0, response.removed_contexts_size());
      }));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    CreateExtensionPageHandler_SyncsInitialLensOverlayStateWhenShowing) {
  EXPECT_CALL(*mock_lens_controller_, IsShowingUI())
      .WillRepeatedly(Return(true));

  ASSERT_TRUE(
      content::ExecJs(web_contents_,
                      "const iframe = document.createElement('iframe'); "
                      "document.body.appendChild(iframe);"));
  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(web_contents_->GetPrimaryMainFrame(), 0);
  ASSERT_NE(child_rfh, nullptr);

  ContextualTasksExtensionHandler::CreateForCurrentDocument(child_rfh);
  auto* child_handler =
      ContextualTasksExtensionHandler::GetForCurrentDocument(child_rfh);
  ASSERT_NE(child_handler, nullptr);

  testing::NiceMock<MockContextualTasksExtensionPage> mock_child_page;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_child_page, OnLensOverlayStateChanged(true))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  mojo::PendingReceiver<mojom::ExtensionPageHandler> child_handler_receiver;
  child_handler->CreateExtensionPageHandler(mock_child_page.BindAndGetRemote(),
                                            std::move(child_handler_receiver));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    ContextualTasksUiService_OnLensOverlayStateChanged_RoutesOnlyToActivePanel) {
  // When the contextual tasks side panel is not open, calling
  // ContextualTasksUiService::OnLensOverlayStateChanged does not route to
  // extension handlers in the tab or backgrounded tasks.
  EXPECT_CALL(mock_page_, OnLensOverlayStateChanged(_)).Times(0);

  auto* ui_service = ContextualTasksUiServiceFactory::GetForBrowserContext(
      browser()->GetProfile());
  ASSERT_TRUE(ui_service);
  ui_service->OnLensOverlayStateChanged(browser(), true, std::nullopt);
  mock_page_.FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_SearchToClientMessageOpenLinkInSidePanelMode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL target_url = embedded_test_server()->GetURL("/title1.html");

  EmbedHandlerInSidePanel();
  ASSERT_TRUE(side_panel_task_id_.has_value());
  content::WebContents* original_tab_contents = web_contents_;

  lens::SearchToClientMessage message;
  message.mutable_open_link_in_side_panel_mode()->set_url(target_url.spec());
  const size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  message.SerializeToArray(serialized_message.data(), size);

  handler_->OnWebviewMessage(serialized_message);

  // The link is opened in a new active tab next to the side panel and
  // associated with the side panel's task.
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(active_contents, nullptr);
  EXPECT_NE(active_contents, original_tab_contents);
  EXPECT_EQ(active_contents->GetVisibleURL(), target_url);

  auto* tasks_service = ContextualTasksServiceFactory::GetForProfile(
      Profile::FromBrowserContext(active_contents->GetBrowserContext()));
  ASSERT_NE(tasks_service, nullptr);
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_contents);
  std::optional<ContextualTask> task =
      tasks_service->GetContextualTaskForTab(active_tab_id);
  ASSERT_TRUE(task.has_value());
  EXPECT_EQ(task->GetTaskId(), *side_panel_task_id_);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_SearchToClientMessageOpenLinkInSidePanelMode_InvalidUrlIgnored) {
  EmbedHandlerInSidePanel();

  // Non-HTTP(S) URL should be ignored.
  {
    lens::SearchToClientMessage message;
    message.mutable_open_link_in_side_panel_mode()->set_url(
        "javascript:alert(1)");
    const size_t size = message.ByteSizeLong();
    std::vector<uint8_t> serialized_message(size);
    message.SerializeToArray(serialized_message.data(), size);
    handler_->OnWebviewMessage(serialized_message);
  }
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            web_contents_);

  // Malformed URL should be ignored.
  {
    lens::SearchToClientMessage message;
    message.mutable_open_link_in_side_panel_mode()->set_url("not a valid url");
    const size_t size = message.ByteSizeLong();
    std::vector<uint8_t> serialized_message(size);
    message.SerializeToArray(serialized_message.data(), size);
    handler_->OnWebviewMessage(serialized_message);
  }
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            web_contents_);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_AimToClientMessageOpenLinkInSidePanelMode) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL target_url = embedded_test_server()->GetURL("/title1.html");

  EmbedHandlerInSidePanel();
  ASSERT_TRUE(side_panel_task_id_.has_value());
  content::WebContents* original_tab_contents = web_contents_;

  lens::AimToClientMessage message;
  message.mutable_open_link_in_side_panel_mode()->set_url(target_url.spec());
  const size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  message.SerializeToArray(serialized_message.data(), size);

  handler_->OnWebviewMessage(serialized_message);

  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(active_contents, nullptr);
  EXPECT_NE(active_contents, original_tab_contents);
  EXPECT_EQ(active_contents->GetVisibleURL(), target_url);

  auto* tasks_service = ContextualTasksServiceFactory::GetForProfile(
      Profile::FromBrowserContext(active_contents->GetBrowserContext()));
  ASSERT_NE(tasks_service, nullptr);
  SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(active_contents);
  std::optional<ContextualTask> task =
      tasks_service->GetContextualTaskForTab(active_tab_id);
  ASSERT_TRUE(task.has_value());
  EXPECT_EQ(task->GetTaskId(), *side_panel_task_id_);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnTabContextStateChanged_NotifiesExtensionPageOnAttachAndRemove) {
  const base::UnguessableToken token = base::UnguessableToken::Create();
  mock_session_handle_->GetUploadedContextTokensForTesting().push_back(token);

  base::RunLoop add_run_loop;
  EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
      .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                    const std::vector<int32_t>& submitted_tab_ids) {
        ASSERT_EQ(tabs.size(), 1u);
        EXPECT_EQ(tabs[0]->tab_id, 101);
        EXPECT_EQ(tabs[0]->title, "Tab One");
        EXPECT_EQ(tabs[0]->url, GURL("https://example.com/one"));
        EXPECT_TRUE(submitted_tab_ids.empty());
        add_run_loop.Quit();
      });

  mock_session_handle_->AddDelayedTabContext(
      token, 101, GURL("https://example.com/one"), "Tab One");
  add_run_loop.Run();

  base::RunLoop remove_run_loop;
  EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
      .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                    const std::vector<int32_t>& submitted_tab_ids) {
        EXPECT_TRUE(tabs.empty());
        EXPECT_TRUE(submitted_tab_ids.empty());
        remove_run_loop.Quit();
      });

  mock_session_handle_->RemoveUploadedContextToken(token);
  remove_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnTabContextStateChanged_DeduplicatesAttachedAndRestoredTabs) {
  const base::UnguessableToken token = base::UnguessableToken::Create();
  mock_session_handle_->GetUploadedContextTokensForTesting().push_back(token);

  base::RunLoop attach_run_loop;
  EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
      .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                    const std::vector<int32_t>& submitted_tab_ids) {
        ASSERT_EQ(tabs.size(), 1u);
        attach_run_loop.Quit();
      });
  mock_session_handle_->AddDelayedTabContext(
      token, 10, GURL("https://example.com/shared"), "Shared Tab");
  attach_run_loop.Run();

  contextual_search::TabInfo duplicate_restored;
  duplicate_restored.tab_id = 99;
  duplicate_restored.url = GURL("https://example.com/shared");
  duplicate_restored.title = "Duplicate Restored";

  contextual_search::TabInfo unique_restored;
  unique_restored.tab_id = 20;
  unique_restored.url = GURL("https://example.com/restored");
  unique_restored.title = "Restored Tab";

  base::RunLoop restore_run_loop;
  EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
      .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                    const std::vector<int32_t>& submitted_tab_ids) {
        ASSERT_EQ(tabs.size(), 2u);
        EXPECT_EQ(tabs[0]->tab_id, 10);
        EXPECT_EQ(tabs[0]->url, GURL("https://example.com/shared"));
        EXPECT_EQ(tabs[1]->tab_id, 20);
        EXPECT_EQ(tabs[1]->url, GURL("https://example.com/restored"));
        EXPECT_THAT(submitted_tab_ids, testing::ElementsAre(20));
        restore_run_loop.Quit();
      });

  mock_session_handle_->SetRestoredTabs({duplicate_restored, unique_restored});
  restore_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       CreateExtensionPageHandler_SyncsInitialTabContextState) {
  const base::UnguessableToken unsubmitted_token =
      base::UnguessableToken::Create();
  const base::UnguessableToken submitted_token =
      base::UnguessableToken::Create();
  mock_session_handle_->GetUploadedContextTokensForTesting().push_back(
      unsubmitted_token);
  mock_session_handle_->GetUploadedContextTokensForTesting().push_back(
      submitted_token);

  mock_session_handle_->AddDelayedTabContext(
      unsubmitted_token, 11, GURL("https://example.com/active"), "Active Tab");
  mock_session_handle_->AddDelayedTabContext(
      submitted_token, 12, GURL("https://example.com/submitted"),
      "Submitted Tab");
  // Simulate `submitted_token` having already been submitted in a prior turn.
  std::erase(mock_session_handle_->GetUploadedContextTokensForTesting(),
             submitted_token);

  contextual_search::TabInfo restored_tab;
  restored_tab.tab_id = 13;
  restored_tab.url = GURL("https://example.com/restored");
  restored_tab.title = "Restored Tab";
  mock_session_handle_->SetRestoredTabs({restored_tab});

  ASSERT_TRUE(
      content::ExecJs(web_contents_,
                      "const iframe = document.createElement('iframe'); "
                      "document.body.appendChild(iframe);"));
  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(web_contents_->GetPrimaryMainFrame(), 0);
  ASSERT_NE(child_rfh, nullptr);

  ContextualTasksExtensionHandler::CreateForCurrentDocument(child_rfh);
  auto* child_handler =
      ContextualTasksExtensionHandler::GetForCurrentDocument(child_rfh);
  ASSERT_NE(child_handler, nullptr);

  testing::NiceMock<MockContextualTasksExtensionPage> mock_child_page;
  base::RunLoop run_loop;
  EXPECT_CALL(mock_child_page, OnTabContextUpdated(_, _))
      .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                    const std::vector<int32_t>& submitted_tab_ids) {
        ASSERT_EQ(tabs.size(), 3u);
        EXPECT_EQ(tabs[0]->tab_id, 11);
        EXPECT_EQ(tabs[1]->tab_id, 12);
        EXPECT_EQ(tabs[2]->tab_id, 13);
        EXPECT_THAT(submitted_tab_ids, testing::ElementsAre(12, 13));
        run_loop.Quit();
      });

  mojo::PendingReceiver<mojom::ExtensionPageHandler> child_handler_receiver;
  child_handler->CreateExtensionPageHandler(mock_child_page.BindAndGetRemote(),
                                            std::move(child_handler_receiver));
  run_loop.Run();
}

class ContextualTasksExtensionHandlerContextLibraryBrowserTest
    : public ContextualTasksExtensionHandlerBrowserTestBase {
 public:
  ContextualTasksExtensionHandlerContextLibraryBrowserTest()
      : ContextualTasksExtensionHandlerBrowserTestBase(
            {kContextualTasks, kContextualTasksRearchitecture,
             kContextualTasksForceEntryPointEligibility,
             kContextualTasksContextLibrary},
            {}) {}

 protected:
  // Adds a webpage context with Chrome tab data to `message`.
  static void AddWebpageContext(
      lens::SearchToClientMessage::UpdateThreadContextLibrary& message,
      int64_t context_id,
      const std::string& url,
      const std::string& title) {
    auto* context = message.add_contexts();
    context->set_context_id(context_id);
    context->mutable_webpage()->set_url(url);
    context->mutable_webpage()->set_title(title);
    context->set_has_chrome_tab_data(true);
  }

  void SendUpdateThreadContextLibrary(
      const lens::SearchToClientMessage::UpdateThreadContextLibrary& library) {
    lens::SearchToClientMessage message;
    *message.mutable_update_thread_context_library() = library;
    const size_t size = message.ByteSizeLong();
    std::vector<uint8_t> serialized_message(size);
    message.SerializeToArray(serialized_message.data(), size);
    handler_->OnWebviewMessage(serialized_message);
  }
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextLibraryBrowserTest,
    OnWebviewMessage_UpdateThreadContextLibrary_SetsRestoredTabsWhenEmpty) {
  handler_->SetTaskId(base::Uuid::GenerateRandomV4());
  ASSERT_TRUE(mock_session_handle_->GetTabContextState().restored.empty());

  lens::SearchToClientMessage::UpdateThreadContextLibrary library;
  AddWebpageContext(library, 1, "https://example.com/a", "Tab A");
  AddWebpageContext(library, 2, "https://example.com/b", "Tab B");
  // Non-tab contexts should not be added as restored tabs.
  auto* pdf_context = library.add_contexts();
  pdf_context->set_context_id(3);
  pdf_context->mutable_pdf()->set_url("https://example.com/doc.pdf");
  pdf_context->mutable_pdf()->set_title("Doc");

  SendUpdateThreadContextLibrary(library);

  const auto& restored = mock_session_handle_->GetTabContextState().restored;
  ASSERT_EQ(restored.size(), 2u);
  EXPECT_EQ(restored[0].url, GURL("https://example.com/a"));
  EXPECT_EQ(restored[0].title, "Tab A");
  EXPECT_TRUE(restored[0].restored_from_aim);
  EXPECT_TRUE(restored[0].submitted);
  EXPECT_EQ(restored[1].url, GURL("https://example.com/b"));
  EXPECT_EQ(restored[1].title, "Tab B");
  EXPECT_TRUE(restored[1].restored_from_aim);
  EXPECT_TRUE(restored[1].submitted);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextLibraryBrowserTest,
    OnWebviewMessage_UpdateThreadContextLibrary_ReplacesOldRestoredTabs) {
  handler_->SetTaskId(base::Uuid::GenerateRandomV4());

  lens::SearchToClientMessage::UpdateThreadContextLibrary first_library;
  AddWebpageContext(first_library, 1, "https://example.com/a", "Tab A");
  AddWebpageContext(first_library, 2, "https://example.com/b", "Tab B");
  SendUpdateThreadContextLibrary(first_library);
  ASSERT_EQ(mock_session_handle_->GetTabContextState().restored.size(), 2u);

  // A later restored tabs update replaces the restored tabs with the server's
  // latest list, in server order.
  lens::SearchToClientMessage::UpdateThreadContextLibrary second_library;
  AddWebpageContext(second_library, 3, "https://example.com/c", "Tab C");
  AddWebpageContext(second_library, 1, "https://example.com/a", "Tab A");
  SendUpdateThreadContextLibrary(second_library);

  const auto& restored = mock_session_handle_->GetTabContextState().restored;
  ASSERT_EQ(restored.size(), 2u);
  EXPECT_EQ(restored[0].url, GURL("https://example.com/c"));
  EXPECT_EQ(restored[0].title, "Tab C");
  EXPECT_EQ(restored[1].url, GURL("https://example.com/a"));
  EXPECT_EQ(restored[1].title, "Tab A");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextLibraryBrowserTest,
    OnWebviewMessage_UpdateThreadContextLibrary_UpdatesServiceAndClearsSubmittedTokens) {
  auto* tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(tasks_service, nullptr);
  ContextualTask task = tasks_service->CreateTask();
  SessionID active_tab_id = sessions::SessionTabHelper::IdForTab(web_contents_);
  tasks_service->AssociateTabWithTask(task.GetTaskId(), active_tab_id);
  handler_->SetTaskId(task.GetTaskId());

  base::UnguessableToken submitted_token = base::UnguessableToken::Create();
  mock_session_handle_->set_submitted_context_tokens({submitted_token});

  contextual_search::FileInfo submitted_file;
  submitted_file.file_token = submitted_token;
  submitted_file.tab_session_id = SessionID::FromSerializedValue(42);
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(101);
  submitted_file.request_id = req_id;
  EXPECT_CALL(*mock_session_handle_, GetSubmittedContextFileInfos())
      .WillOnce(
          Return(std::vector<contextual_search::FileInfo>{submitted_file}));

  lens::SearchToClientMessage::UpdateThreadContextLibrary library;
  AddWebpageContext(library, 101, "https://example.com/a", "Tab A");
  auto* pdf_context = library.add_contexts();
  pdf_context->set_context_id(202);
  pdf_context->mutable_pdf()->set_url("https://example.com/doc.pdf");
  pdf_context->mutable_pdf()->set_title("Doc");

  SendUpdateThreadContextLibrary(library);

  // Submitted tokens are cleared once committed by the server.
  EXPECT_TRUE(mock_session_handle_->GetSubmittedContextTokens().empty());

  // The matching submitted FileInfo populates `tab_id` on the restored tab.
  const auto& restored = mock_session_handle_->GetTabContextState().restored;
  ASSERT_EQ(restored.size(), 1u);
  EXPECT_EQ(restored[0].tab_id, 42);
  EXPECT_EQ(restored[0].url, GURL("https://example.com/a"));
  EXPECT_EQ(restored[0].title, "Tab A");

  // ContextualTasksService receives all committed URL resources (both tab and
  // PDF). This is for underlining, etc.
  std::optional<ContextualTask> updated_task =
      tasks_service->GetContextualTaskForTab(active_tab_id);
  ASSERT_TRUE(updated_task.has_value());
  const std::vector<UrlResource> resources = updated_task->GetUrlResources();
  ASSERT_EQ(resources.size(), 2u);
  EXPECT_EQ(resources[0].url, GURL("https://example.com/a"));
  EXPECT_EQ(resources[0].title, "Tab A");
  EXPECT_EQ(resources[0].tab_id, SessionID::FromSerializedValue(42));
  EXPECT_TRUE(resources[0].has_chrome_tab_data);
  EXPECT_EQ(resources[1].url, GURL("https://example.com/doc.pdf"));
  EXPECT_EQ(resources[1].title, "Doc");
  EXPECT_FALSE(resources[1].has_chrome_tab_data);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextLibraryBrowserTest,
    OnWebviewMessage_UpdateThreadContextLibrary_EmptyMessageReturnsEarly) {
  auto* tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(tasks_service, nullptr);
  ContextualTask task = tasks_service->CreateTask();
  SessionID active_tab_id = sessions::SessionTabHelper::IdForTab(web_contents_);
  tasks_service->AssociateTabWithTask(task.GetTaskId(), active_tab_id);
  handler_->SetTaskId(task.GetTaskId());

  // Seed existing restored tabs and submitted tokens.
  lens::SearchToClientMessage::UpdateThreadContextLibrary initial_library;
  AddWebpageContext(initial_library, 1, "https://example.com/a", "Tab A");
  SendUpdateThreadContextLibrary(initial_library);
  ASSERT_EQ(mock_session_handle_->GetTabContextState().restored.size(), 1u);

  base::UnguessableToken submitted_token = base::UnguessableToken::Create();
  mock_session_handle_->set_submitted_context_tokens({submitted_token});

  // Sending an empty UpdateThreadContextLibrary returns early without clearing
  // submitted tokens or overwriting existing restored tabs / service resources.
  lens::SearchToClientMessage::UpdateThreadContextLibrary empty_library;
  SendUpdateThreadContextLibrary(empty_library);

  EXPECT_THAT(mock_session_handle_->GetSubmittedContextTokens(),
              testing::ElementsAre(submitted_token));
  EXPECT_EQ(mock_session_handle_->GetTabContextState().restored.size(), 1u);
  std::optional<ContextualTask> updated_task =
      tasks_service->GetContextualTaskForTab(active_tab_id);
  ASSERT_TRUE(updated_task.has_value());
  EXPECT_EQ(updated_task->GetUrlResources().size(), 1u);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextLibraryBrowserTest,
    OnWebviewMessage_UpdateThreadContextLibrary_NoTaskIdOrNoSessionHandleReturnsEarly) {
  auto* tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(tasks_service, nullptr);
  ContextualTask task = tasks_service->CreateTask();
  SessionID active_tab_id = sessions::SessionTabHelper::IdForTab(web_contents_);
  tasks_service->AssociateTabWithTask(task.GetTaskId(), active_tab_id);

  base::UnguessableToken submitted_token = base::UnguessableToken::Create();
  mock_session_handle_->set_submitted_context_tokens({submitted_token});

  lens::SearchToClientMessage::UpdateThreadContextLibrary library;
  AddWebpageContext(library, 1, "https://example.com/a", "Tab A");

  // 1. Before SetTaskId() is called, the update is ignored.
  SendUpdateThreadContextLibrary(library);
  EXPECT_TRUE(mock_session_handle_->GetTabContextState().restored.empty());
  EXPECT_THAT(mock_session_handle_->GetSubmittedContextTokens(),
              testing::ElementsAre(submitted_token));

  // 2. With task_id set but no session handle in the helper, the update returns
  // early without updating ContextualTasksService.
  handler_->SetTaskId(task.GetTaskId());
  mock_session_handle_ = nullptr;
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(task.GetTaskId(), /*handle=*/nullptr,
                       /*input_state_model=*/nullptr);

  SendUpdateThreadContextLibrary(library);
  std::optional<ContextualTask> updated_task =
      tasks_service->GetContextualTaskForTab(active_tab_id);
  ASSERT_TRUE(updated_task.has_value());
  EXPECT_TRUE(updated_task->GetUrlResources().empty());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    ZeroIframes_PostSearchMessageQueuesUntilHandshakeAndFlushes) {
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  // Unregister the primary extension frame handler to simulate 0 extension
  // iframes mounted in the page.
  user_data->UnregisterExtensionFrame(handler_);
  EXPECT_FALSE(user_data->HasBoundExtensionFrame());

  std::vector<lens::ClientToSearchMessage> dispatched_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage& message) {
        dispatched_messages.push_back(message);
      }));

  // Trigger Lens crop while 0 iframes are mounted and before
  // OnDocumentConnected or handshake completes; the outbound messages
  // (InjectChromeInput and OnContextStateChanged) should be queued and must not
  // be duplicated when OnDocumentConnected and OnHandshakeComplete run.
  user_data->OnLensThumbnailCreated("data:image/png;base64,zero_iframe_crop");
  auto model = user_data->GetOrCreateInputStateModel();
  ASSERT_TRUE(model && model->lens_crop().has_value());
  EXPECT_EQ("data:image/png;base64,zero_iframe_crop",
            model->lens_crop()->data_uri);
  EXPECT_TRUE(dispatched_messages.empty());

  // Connect the AIM document via Service Worker Port (connectDocument).
  user_data->OnDocumentConnected(web_contents_->GetPrimaryMainFrame());
  EXPECT_TRUE(user_data->IsServiceWorkerPortConnected());
  EXPECT_FALSE(user_data->IsHandshakeCompleteForTesting());
  EXPECT_TRUE(dispatched_messages.empty());

  // Now complete the handshake via SearchToClientMessage.HandshakeResponse.
  EXPECT_CALL(*mock_controller_, SetAuthUserIndex(3));
  lens::SearchToClientMessage handshake_resp;
  handshake_resp.mutable_handshake_response()->set_auth_user_index(3);
  std::string resp_str;
  ASSERT_TRUE(handshake_resp.SerializeToString(&resp_str));
  std::vector<uint8_t> resp_bytes(resp_str.begin(), resp_str.end());

  EXPECT_TRUE(user_data->OnSearchMessageReceived(resp_bytes));
  EXPECT_TRUE(user_data->IsHandshakeCompleteForTesting());
  EXPECT_EQ(mock_session_handle_->auth_user_index(), 3u);
  ASSERT_EQ(dispatched_messages.size(), 2u);
  EXPECT_TRUE(dispatched_messages[0].has_inject_chrome_input());
  EXPECT_EQ(dispatched_messages[0].inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
  EXPECT_TRUE(dispatched_messages[0].inject_chrome_input().is_active());
  ASSERT_TRUE(dispatched_messages[1].has_on_context_state_changed());
  EXPECT_EQ(
      dispatched_messages[1].on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);

  // RemoveLensCrop with 0 iframes should also succeed and clear the crop.
  dispatched_messages.clear();
  user_data->RemoveLensCrop();
  EXPECT_FALSE(model->GetLensCrop().has_value());
  ASSERT_EQ(dispatched_messages.size(), 2u);
  EXPECT_FALSE(dispatched_messages[0].inject_chrome_input().is_active());
  ASSERT_TRUE(dispatched_messages[1].has_on_context_state_changed());
  EXPECT_EQ(
      dispatched_messages[1].on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_NONE);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       ZeroIframes_PageNavigationResetsPerPageSearchState) {
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  user_data->UnregisterExtensionFrame(handler_);
  handler_ = nullptr;
  EXPECT_FALSE(user_data->HasBoundExtensionFrame());

  user_data->OnDocumentConnected(web_contents_->GetPrimaryMainFrame());
  user_data->OnHandshakeComplete();
  EXPECT_TRUE(user_data->IsServiceWorkerPortConnected());
  EXPECT_TRUE(user_data->IsHandshakeCompleteForTesting());
  EXPECT_FALSE(user_data->GetConnectedDocumentIdForTesting().empty());

  std::vector<lens::ClientToSearchMessage> dispatched_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage& message) {
        dispatched_messages.push_back(message);
      }));

  // Mount a Lens crop on the initial page.
  user_data->OnLensThumbnailCreated("data:image/png;base64,page1_crop");
  ASSERT_EQ(dispatched_messages.size(), 2u);
  dispatched_messages.clear();

  // Navigate to a new primary page. Per-page search state (stored on
  // content::PageUserData) must immediately reflect the new page before
  // OnDocumentConnected() is called for the new document.
  ASSERT_TRUE(embedded_test_server()->Start());
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  EXPECT_FALSE(user_data->IsServiceWorkerPortConnected());
  EXPECT_FALSE(user_data->IsHandshakeCompleteForTesting());
  EXPECT_TRUE(user_data->GetConnectedDocumentIdForTesting().empty());

  // Completing handshake on the new page should re-mount the active Lens crop
  // and re-emit CONTEXT_STATE_READY on the new page.
  user_data->OnDocumentConnected(web_contents_->GetPrimaryMainFrame());
  EXPECT_TRUE(user_data->IsServiceWorkerPortConnected());
  EXPECT_FALSE(user_data->IsHandshakeCompleteForTesting());
  user_data->OnHandshakeComplete();
  EXPECT_TRUE(user_data->IsHandshakeCompleteForTesting());
  ASSERT_EQ(dispatched_messages.size(), 2u);
  EXPECT_TRUE(dispatched_messages[0].has_inject_chrome_input());
  EXPECT_EQ(dispatched_messages[0].inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
  EXPECT_TRUE(dispatched_messages[0].inject_chrome_input().is_active());
  ASSERT_TRUE(dispatched_messages[1].has_on_context_state_changed());
  EXPECT_EQ(
      dispatched_messages[1].on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       ZeroIframes_OnSubmitQueryRequest) {
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  // Unregister the primary extension frame handler to simulate 0 extension
  // iframes mounted in the page.
  user_data->UnregisterExtensionFrame(handler_);
  EXPECT_FALSE(user_data->HasBoundExtensionFrame());

  user_data->OnDocumentConnected(web_contents_->GetPrimaryMainFrame());
  user_data->OnHandshakeComplete();
  EXPECT_TRUE(user_data->IsHandshakeCompleteForTesting());

  std::vector<lens::ClientToSearchMessage> dispatched_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage& message) {
        dispatched_messages.push_back(message);
      }));

  // Verify OnSubmitQueryRequest is handled with 0 iframes.
  contextual_search::FileInfo file_info;
  file_info.file_token = base::UnguessableToken::Create();
  AttachActiveTab(file_info.file_token);
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(777);
  file_info.request_id = req_id;
  file_info.input_data = std::make_unique<lens::ContextualInputData>();
  file_info.input_data->upload_type =
      lens::LensOverlayContextualInputUploadType::
          CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;

  ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(
          Return(std::vector<contextual_search::FileInfo>{file_info}));
  ON_CALL(*mock_session_handle_, search_session_id())
      .WillByDefault(Return("zero_iframe_session"));

  lens::SearchToClientMessage submit_msg;
  submit_msg.mutable_on_submit_query_request();
  std::string submit_str;
  ASSERT_TRUE(submit_msg.SerializeToString(&submit_str));

  SimulateUserInteraction();
  EXPECT_TRUE(user_data->OnSearchMessageReceived(
      std::vector<uint8_t>(submit_str.begin(), submit_str.end())));
  ASSERT_EQ(dispatched_messages.size(), 1u);
  ASSERT_TRUE(dispatched_messages.back().has_on_submit_query_response());
  const auto& resp = dispatched_messages.back().on_submit_query_response();
  ASSERT_EQ(resp.added_contexts_size(), 1);
  EXPECT_EQ(resp.added_contexts(0).search_session_id(), "zero_iframe_session");
  EXPECT_EQ(resp.added_contexts(0).request_id().context_id(), 777);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnContextStateChanged_TabUploadLifecycleAndSubmissionResetsToNone) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t tab_id = active_tab->GetHandle().raw_value();

  // 1. Adding a tab emits CONTEXT_LIBRARY mount followed by
  // CONTEXT_STATE_UPLOADING.
  base::RunLoop add_run_loop;
  base::UnguessableToken token;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_inject_chrome_input());
        EXPECT_EQ(
            message->inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
        EXPECT_TRUE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_UPLOADING);
        add_run_loop.Quit();
      });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            token = result.value();
          }));
  add_run_loop.Run();

  // 2. When the tab finishes uploading, OnContextStateChanged emits
  // CONTEXT_STATE_READY.
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  ASSERT_NE(user_data, nullptr);

  base::RunLoop ready_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        ready_run_loop.Quit();
      });

  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  ready_run_loop.Run();

  // 3. Submitting the query moves the tab into persisted/submitted context and
  // emits CONTEXT_STATE_NONE (previous-turn context is excluded).
  contextual_search::FileInfo file_info;
  file_info.file_token = token;
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(999);
  file_info.request_id = req_id;
  ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(
          Return(std::vector<contextual_search::FileInfo>{file_info}));

  base::RunLoop submit_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_on_submit_query_response());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_inject_chrome_input());
        EXPECT_EQ(
            message->inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
        EXPECT_FALSE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
        submit_run_loop.Quit();
      });

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message);
  submit_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnContextStateChanged_DelayedTabSnapshotTransitionsToReady) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t tab_id = active_tab->GetHandle().raw_value();

  base::RunLoop ready_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_inject_chrome_input());
        EXPECT_EQ(
            message->inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
        EXPECT_TRUE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_UPLOADING);
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        ready_run_loop.Quit();
      });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      tab_id, /*delay_upload=*/true,
      searchbox::mojom::TabAttachmentSource::kContextMenu, base::DoNothing());
  ready_run_loop.Run();

  base::RunLoop delete_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_inject_chrome_input());
        EXPECT_FALSE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
        delete_run_loop.Quit();
      });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      tab_id);
  delete_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnContextStateChanged_UploadingTakesPrecedenceOverReadyLensCrop) {
  // 1. Mount a Lens crop -> CONTEXT_STATE_READY.
  base::RunLoop crop_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        crop_run_loop.Quit();
      });
  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,test_crop");
  crop_run_loop.Run();

  // 2. Add a tab that starts uploading -> transitions from READY to UPLOADING.
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t tab_id = active_tab->GetHandle().raw_value();

  base::RunLoop tab_add_run_loop;
  base::UnguessableToken token;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_UPLOADING);
        tab_add_run_loop.Quit();
      });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            token = result.value();
          }));
  tab_add_run_loop.Run();

  // 3. If the tab upload fails, the ready Lens crop keeps the state at READY.
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  ASSERT_NE(user_data, nullptr);

  base::RunLoop fail_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        fail_run_loop.Quit();
      });

  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadFailed,
      contextual_search::ContextUploadErrorType::kNetworkError);
  fail_run_loop.Run();

  // 4. Removing the Lens crop leaves only the failed tab upload, transitioning
  // to CONTEXT_STATE_NONE.
  base::RunLoop remove_crop_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
        EXPECT_FALSE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
        remove_crop_run_loop.Quit();
      });

  handler_->RemoveLensCrop();
  remove_crop_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnContextStateChanged_ObserverLifecycleHandshakeAndNonTerminalStatus) {
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  ASSERT_NE(user_data, nullptr);

  // 1. Configure `mock_controller_->AsWeakPtr()` to return a valid WeakPtr and
  // verify `ObserveContextController` registers `user_data` once (and is
  // idempotent on repeated calls for the same controller).
  base::WeakPtrFactory<contextual_search::ContextualSearchContextController>
      controller1_weak_factory(mock_controller_.get());
  ON_CALL(*mock_controller_, AsWeakPtr()).WillByDefault([&]() {
    return controller1_weak_factory.GetWeakPtr();
  });

  EXPECT_CALL(*mock_controller_, AddObserver(user_data)).Times(1);
  EXPECT_EQ(user_data->GetOrCreateContextualSessionHandle(),
            mock_session_handle_);
  EXPECT_EQ(user_data->GetOrCreateContextualSessionHandle(),
            mock_session_handle_);
  testing::Mock::VerifyAndClearExpectations(mock_controller_.get());

  // 2. Add a tab (starts in UPLOADING), then fail it (transitions to NONE),
  // then send a non-terminal status (`kProcessing`) to verify it clears the
  // failed status and transitions back to UPLOADING, and finally complete it
  // (`kUploadSuccessful`) to transition to READY.
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t tab_id = active_tab->GetHandle().raw_value();

  base::RunLoop add_run_loop;
  base::UnguessableToken token;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_UPLOADING);
        add_run_loop.Quit();
      });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            token = result.value();
          }));
  add_run_loop.Run();

  base::RunLoop fail_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
        fail_run_loop.Quit();
      });
  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadFailed,
      contextual_search::ContextUploadErrorType::kNetworkError);
  fail_run_loop.Run();

  // Non-terminal status (`kProcessing`) clears `failed_context_uploads_` and
  // re-inserts into `pending_context_uploads_`, emitting UPLOADING.
  base::RunLoop processing_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_UPLOADING);
        processing_run_loop.Quit();
      });
  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kProcessing, std::nullopt);
  processing_run_loop.Run();

  base::RunLoop ready_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        ready_run_loop.Quit();
      });
  user_data->OnContextUploadStatusChanged(
      token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  ready_run_loop.Run();

  // 3. Receiving a handshake response while a tab is selected mounts
  // CONTEXT_LIBRARY and re-emits the current context state
  // (CONTEXT_STATE_READY) even though CONTEXT_STATE_READY was already sent
  // prior to the handshake.
  base::RunLoop handshake_run_loop;
  EXPECT_CALL(mock_page_, OnHandshakeComplete());
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_inject_chrome_input());
        EXPECT_EQ(
            message->inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
        EXPECT_TRUE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        handshake_run_loop.Quit();
      });

  lens::SearchToClientMessage handshake_msg;
  handshake_msg.mutable_handshake_response()->set_auth_user_index(0);
  const size_t size = handshake_msg.ByteSizeLong();
  std::vector<uint8_t> serialized_handshake(size);
  handshake_msg.SerializeToArray(serialized_handshake.data(), size);
  handler_->OnWebviewMessage(serialized_handshake);
  handshake_run_loop.Run();

  // 4. Switching controllers unregisters from the old controller and registers
  // on the new controller; destroying `ContextualTasksWebContentsUserData`
  // unregisters from the active observed controller.
  NiceMock<contextual_search::MockContextualSearchContextController>
      second_controller;
  base::WeakPtrFactory<contextual_search::ContextualSearchContextController>
      controller2_weak_factory(&second_controller);
  ON_CALL(second_controller, AsWeakPtr()).WillByDefault([&]() {
    return controller2_weak_factory.GetWeakPtr();
  });
  ON_CALL(*mock_session_handle_, GetController())
      .WillByDefault(Return(&second_controller));

  EXPECT_CALL(*mock_controller_, RemoveObserver(user_data)).Times(1);
  EXPECT_CALL(second_controller, AddObserver(user_data)).Times(1);
  EXPECT_EQ(user_data->GetOrCreateContextualSessionHandle(),
            mock_session_handle_);
  testing::Mock::VerifyAndClearExpectations(mock_controller_.get());
  testing::Mock::VerifyAndClearExpectations(&second_controller);

  EXPECT_CALL(second_controller, RemoveObserver(user_data)).Times(1);
  web_contents_->RemoveUserData(
      ContextualTasksWebContentsUserData::UserDataKey());
}

}  // namespace contextual_tasks
