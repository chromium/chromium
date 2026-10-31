// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_extension_handler.h"

#include <algorithm>
#include <map>
#include <memory>
#include <variant>

#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_extension_bridge.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_page.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/interaction/browser_elements.h"
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
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
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
#include "content/public/test/test_utils.h"
#include "extensions/browser/extension_api_frame_id_map.h"
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
#include "ui/base/interaction/element_tracker.h"
#include "ui/base/unowned_user_data/user_data_factory.h"
#include "ui/events/base_event_utils.h"
#include "ui/gfx/geometry/rect_f.h"
#include "ui/webui/resources/js/tracked_element/tracked_element.mojom.h"

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
    // Expectation actions may reference test-body stack state (e.g. RunLoop
    // quit closures). Clear them before AssertThatNetworkServiceDidNotCrash()
    // spins a run loop that can dispatch late mojo messages to these mocks.
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
    testing::Mock::VerifyAndClearExpectations(&mock_panel_page_);
    testing::Mock::VerifyAndClearExpectations(&mock_searchbox_page_);
    mock_lens_overlay_controller_.reset();
    if (mock_lens_controller_) {
      testing::Mock::VerifyAndClearExpectations(mock_lens_controller_);
      mock_lens_controller_ = nullptr;
    }
    composebox_handler_remote_.reset();
    handler_ = nullptr;
    panel_web_contents_ = nullptr;
    mock_session_handle_ = nullptr;
    if (side_panel_task_id_.has_value()) {
      ContextualTasksPanelController::From(browser())->DetachWebContentsForTask(
          *side_panel_task_id_);
      side_panel_task_id_.reset();
    }
    // Don't reset `mock_controller_` here. The mock session handle stays on
    // the tab until browser shutdown, which runs after this method returns,
    // and keeps returning `mock_controller_` from `GetController()`.
    // `AddTabContext()` starts an async tab screenshot capture, and its upload
    // callback can run during that shutdown and call `GetController()`.
    // `mock_controller_` is destroyed with the fixture, after the browser is
    // gone.
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
    panel_web_contents_ = raw_panel_contents;
    auto panel_session = std::make_unique<
        NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
    mock_session_handle_ = panel_session.get();
    ON_CALL(*mock_session_handle_, GetController())
        .WillByDefault(Return(mock_controller_.get()));
    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
        raw_panel_contents)
        ->SetTaskSession(*side_panel_task_id_, std::move(panel_session),
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

  void ShowSidePanel() {
    ContextualTasksPanelController::From(browser())->Show(
        /*transition_from_tab=*/false,
        omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
        /*use_no_animation=*/true);
  }

  void SendSearchMessage(const lens::SearchToClientMessage& msg) {
    std::vector<uint8_t> bytes(msg.ByteSizeLong());
    msg.SerializeToArray(bytes.data(), bytes.size());
    handler_->OnWebviewMessage(bytes, std::nullopt);
  }

  void SendOnPageChanged(bool is_zero_state) {
    lens::SearchToClientMessage msg;
    msg.mutable_on_page_changed()->set_is_zero_state(is_zero_state);
    SendSearchMessage(msg);
  }

  ContextualTasksWebContentsUserData* StartTestServerAndOpenSidePanel(
      GURL* url1_out = nullptr,
      GURL* url2_out = nullptr) {
    EXPECT_TRUE(embedded_test_server()->Start());
    const GURL url1 = embedded_test_server()->GetURL("/title1.html");
    if (url1_out) {
      *url1_out = url1;
    }
    if (url2_out) {
      *url2_out = embedded_test_server()->GetURL("/title2.html");
    }
    handler_ = nullptr;
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
    EmbedHandlerInSidePanel();
    ShowSidePanel();
    return ContextualTasksWebContentsUserData::FromWebContents(
        panel_web_contents_);
  }

  void AllowSessionTokenCreation() {
    auto* session = mock_session_handle_.get();
    ON_CALL(*mock_session_handle_, CreateContextToken())
        .WillByDefault([session]() {
          auto token = base::UnguessableToken::Create();
          session->GetUploadedContextTokensForTesting().push_back(token);
          return token;
        });
  }

  void AttachActiveTab(const base::UnguessableToken& token) {
    SessionID active_tab_id =
        sessions::SessionTabHelper::IdForTab(web_contents_);
    mock_session_handle_->GetUploadedContextTokensForTesting().push_back(token);
    mock_session_handle_->AttachTabPendingUpload(
        token, active_tab_id.id(), web_contents_->GetLastCommittedURL(),
        "Test Tab");
    mock_page_.FlushForTesting();
  }

 protected:
  ui::UserDataFactory::ScopedOverride lens_controller_override_;
  raw_ptr<MockLensSearchController> mock_lens_controller_ = nullptr;
  std::unique_ptr<NiceMock<MockLensOverlayController>>
      mock_lens_overlay_controller_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<content::WebContents> panel_web_contents_ = nullptr;
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
             kContextualTasksSidePanelRearchitecture,
             kContextualTasksForceEntryPointEligibility},
            {kContextualTasksClobberActiveTab}) {}
};

class ContextualTasksExtensionHandlerClobberActiveTabBrowserTest
    : public ContextualTasksExtensionHandlerBrowserTestBase {
 public:
  ContextualTasksExtensionHandlerClobberActiveTabBrowserTest()
      : ContextualTasksExtensionHandlerBrowserTestBase(
            {kContextualTasks, kContextualTasksRearchitecture,
             kContextualTasksSidePanelRearchitecture,
             kContextualTasksForceEntryPointEligibility,
             kContextualTasksClobberActiveTab},
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
      WindowOpenDisposition::kNewForegroundTab,
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
      WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com/test2"),
      WindowOpenDisposition::kNewForegroundTab,
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
                       OnSessionChanged_SendsNewSessionTabsToExtensionPage) {
  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents_);

  auto new_session = std::make_unique<
      NiceMock<contextual_search::MockContextualSearchSessionHandle>>();
  auto* new_session_ptr = new_session.get();
  ON_CALL(*new_session_ptr, GetController())
      .WillByDefault(Return(mock_controller_.get()));

  base::UnguessableToken token = base::UnguessableToken::Create();
  new_session_ptr->GetUploadedContextTokensForTesting().push_back(token);
  new_session_ptr->AttachTabPendingUpload(token, /*tab_id=*/42,
                                          GURL("https://example.com/switched"),
                                          "Switched Tab");

  base::RunLoop set_session_loop;
  EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
      .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                    std::vector<int32_t> submitted_tab_ids) {
        ASSERT_EQ(tabs.size(), 1u);
        EXPECT_EQ(tabs[0]->tab_id, 42);
        EXPECT_EQ(tabs[0]->url, GURL("https://example.com/switched"));
        EXPECT_EQ(tabs[0]->title, "Switched Tab");
        EXPECT_TRUE(submitted_tab_ids.empty());
        set_session_loop.Quit();
      });

  // Setting a new session on the helper should notify `handler_` via
  // `OnSessionChanged()` and send the new session's tabs to the extension page.
  mock_session_handle_ = nullptr;
  helper->SetTaskSession(std::nullopt, std::move(new_session),
                         /*input_state_model=*/nullptr);
  set_session_loop.Run();

  // Taking the session handle should notify `OnSessionChanged(nullptr)` and
  // send an empty tab list to the extension page.
  base::RunLoop take_session_loop;
  EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
      .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                    std::vector<int32_t> submitted_tab_ids) {
        EXPECT_TRUE(tabs.empty());
        EXPECT_TRUE(submitted_tab_ids.empty());
        take_session_loop.Quit();
      });

  auto taken_session = helper->TakeSessionHandle();
  ASSERT_TRUE(taken_session);
  take_session_loop.Run();
}

// Runs the submit-time QueryContextualizer against a mock page context for the
// active tab and a mock ContextualTasksService. Uploads are forwarded to the
// session handle, which updates its attached tabs, and succeed right away.
class ContextualTasksExtensionHandlerContextualizerBrowserTest
    : public ContextualTasksExtensionHandlerBrowserTest {
 public:
  using UploadType = lens::LensOverlayContextualInputUploadType;

  ContextualTasksExtensionHandlerContextualizerBrowserTest() {
    // Enables `omnibox::IsTabDeselectionInComposeboxEnabled()`.
    deselection_feature_list_.InitAndEnableFeatureWithParameters(
        omnibox::kContextManagementInComposebox,
        {{"enable_tab_deselection", "true"}});
  }

  void SetUpOnMainThread() override {
    ContextualTasksExtensionHandlerBrowserTest::SetUpOnMainThread();
    active_tab_ = TabListInterface::From(browser())->GetActiveTab();
    active_session_id_ =
        sessions::SessionTabHelper::IdForTab(active_tab_->GetContents());
    active_url_ = active_tab_->GetContents()->GetLastCommittedURL();
    active_title_ = base::UTF16ToUTF8(active_tab_->GetTitle());

    active_tab_->GetTabFeatures()->SetTabContextualizationControllerForTesting(
        nullptr);
    auto tab_controller =
        std::make_unique<NiceMock<MockTabContextualizationController>>(
            active_tab_);
    mock_tab_controller_ = tab_controller.get();
    active_tab_->GetTabFeatures()->SetTabContextualizationControllerForTesting(
        std::move(tab_controller));
    SetActiveTabPageContext({1, 2, 3});

    contextual_tasks::ContextualTasksWebContentsUserData::
        GetOrCreateForWebContents(web_contents_)
            ->SetContextualTasksServiceForTesting(&mock_tasks_service_);

    // Let the QueryContextualizer track upload statuses.
    controller_weak_factory_ = std::make_unique<base::WeakPtrFactory<
        contextual_search::ContextualSearchContextController>>(
        mock_controller_.get());
    ON_CALL(*mock_controller_, AsWeakPtr()).WillByDefault([this]() {
      return controller_weak_factory_->GetWeakPtr();
    });
    ON_CALL(*mock_controller_, AddObserver(_))
        .WillByDefault([this](auto* observer) {
          upload_observers_.AddObserver(observer);
        });
    ON_CALL(*mock_controller_, RemoveObserver(_))
        .WillByDefault([this](auto* observer) {
          upload_observers_.RemoveObserver(observer);
        });
    ON_CALL(*mock_controller_, GetFileInfoList()).WillByDefault([this]() {
      std::vector<raw_ptr<const contextual_search::FileInfo>> infos;
      if (previous_upload_) {
        infos.push_back(previous_upload_.get());
      }
      return infos;
    });

    ON_CALL(*mock_session_handle_, StartTabContextUploadFlow(_, _, _))
        .WillByDefault(
            [this](const base::UnguessableToken& token,
                   std::unique_ptr<lens::ContextualInputData> data,
                   std::optional<lens::ImageEncodingOptions> options) {
              upload_types_[token] = *data->upload_type;
              // Like the real controller, a re-upload of the tab supersedes
              // its previous upload.
              if (previous_upload_ &&
                  data->tab_session_id == previous_upload_->tab_session_id) {
                previous_upload_->is_superceded = true;
              }
              mock_session_handle_->contextual_search::
                  ContextualSearchSessionHandle::StartTabContextUploadFlow(
                      token, std::move(data), options);
              for (auto& observer : upload_observers_) {
                observer.OnContextUploadStatusChanged(
                    token, lens::MimeType::kAnnotatedPageContent,
                    contextual_search::ContextUploadStatus::kUploadSuccessful,
                    std::nullopt);
              }
            });
    ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
        .WillByDefault([this]() {
          std::vector<contextual_search::FileInfo> infos;
          for (const auto& [token, upload_type] : upload_types_) {
            contextual_search::FileInfo& info = infos.emplace_back();
            info.file_token = token;
            info.request_id = lens::LensOverlayRequestId();
            info.input_data = std::make_unique<lens::ContextualInputData>();
            info.input_data->upload_type = upload_type;
          }
          return infos;
        });
  }

  void TearDownOnMainThread() override {
    contextual_tasks::ContextualTasksWebContentsUserData::
        GetOrCreateForWebContents(web_contents_)
            ->SetContextualTasksServiceForTesting(nullptr);
    mock_tab_controller_ = nullptr;
    active_tab_->GetTabFeatures()->SetTabContextualizationControllerForTesting(
        nullptr);
    active_tab_ = nullptr;
    ContextualTasksExtensionHandlerBrowserTest::TearDownOnMainThread();
    controller_weak_factory_.reset();
  }

  // Makes the active tab's page context have `viewport_bytes` as content.
  void SetActiveTabPageContext(std::vector<uint8_t> viewport_bytes) {
    ON_CALL(*mock_tab_controller_, GetPageContext(_))
        .WillByDefault([this, viewport_bytes](auto callback) {
          auto data = std::make_unique<lens::ContextualInputData>();
          data->tab_session_id = active_session_id_;
          data->page_url = active_url_;
          data->page_title = active_title_;
          data->is_page_context_eligible = true;
          data->viewport_screenshot_bytes = viewport_bytes;
          std::move(callback).Run(std::move(data));
        });
  }

  // Adds the active tab as an auto-suggested chip and returns its chip token.
  base::UnguessableToken AddActiveTabChip() {
    base::UnguessableToken chip_token;
    static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
        active_tab_->GetHandle().raw_value(), /*delay_upload=*/true,
        searchbox::mojom::TabAttachmentSource::kContextMenu,
        base::BindLambdaForTesting(
            [&](base::expected<base::UnguessableToken,
                               contextual_search::ContextUploadErrorType>
                    result) { chip_token = result.value(); }));
    EXPECT_FALSE(chip_token.is_empty());
    return chip_token;
  }

  // Attaches the active tab as if it was uploaded in a previous turn with
  // `viewport_bytes` as content, and makes the task context contain it.
  // The task id is set via `SetTaskId()`, or only on the WebContents'
  // `ContextualSearchWebContentsHelper` (like the side panel) if
  // `task_id_from_helper`. Returns the upload's token.
  base::UnguessableToken SeedUploadedActiveTab(
      std::vector<uint8_t> viewport_bytes,
      bool task_id_from_helper = false) {
    const base::UnguessableToken token = base::UnguessableToken::Create();
    AttachActiveTab(token);
    upload_types_[token] = UploadType::CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT;
    previous_upload_ = std::make_unique<contextual_search::FileInfo>();
    previous_upload_->file_token = token;
    previous_upload_->tab_session_id = active_session_id_;
    previous_upload_->upload_status =
        contextual_search::ContextUploadStatus::kUploadSuccessful;
    previous_upload_->request_id = lens::LensOverlayRequestId();
    previous_upload_->request_id->set_context_id(12345);
    previous_upload_->input_data =
        std::make_unique<lens::ContextualInputData>();
    previous_upload_->input_data->tab_session_id = active_session_id_;
    previous_upload_->input_data->page_url = active_url_;
    previous_upload_->input_data->viewport_screenshot_bytes =
        std::move(viewport_bytes);
    // Superseded files keep their tab.
    ON_CALL(*mock_controller_, GetFileInfo(token))
        .WillByDefault(Return(previous_upload_.get()));

    task_id_ = base::Uuid::GenerateRandomV4();
    if (task_id_from_helper) {
      // Assigns `task_id_` to the helper's (untasked) session.
      EXPECT_TRUE(
          ContextualSearchWebContentsHelper::FromWebContents(web_contents_)
              ->GetSessionForTask(task_id_));
    } else {
      handler_->SetTaskId(task_id_);
    }
    ON_CALL(mock_tasks_service_, GetContextForTask(task_id_, _, _, _))
        .WillByDefault([this](auto, auto, auto, auto callback) {
          ContextualTask task(task_id_);
          UrlResource resource(active_url_, ResourceType::kWebpage);
          resource.title = active_title_;
          resource.tab_id = active_session_id_;
          task.AddUrlResource(resource);
          std::move(callback).Run(
              std::make_unique<ContextualTaskContext>(task));
        });
    return token;
  }

  // Submits a query and returns the upload types of the added contexts of the
  // response.
  std::vector<UploadType> Submit() {
    std::vector<UploadType> upload_types;
    base::RunLoop run_loop;
    EXPECT_CALL(mock_page_, PostSearchMessage(_))
        .WillRepeatedly([&](mojo_base::ProtoWrapper wrapper) {
          auto message = wrapper.As<lens::ClientToSearchMessage>();
          if (!message.has_value() ||
              !message->has_on_submit_query_response()) {
            return;
          }
          for (const auto& added :
               message->on_submit_query_response().added_contexts()) {
            upload_types.push_back(added.contextual_input_upload_type());
          }
          run_loop.Quit();
        });
    lens::SearchToClientMessage request;
    request.mutable_on_submit_query_request();
    std::vector<uint8_t> serialized_request(request.ByteSizeLong());
    request.SerializeToArray(serialized_request.data(),
                             serialized_request.size());
    SimulateUserInteraction();
    handler_->OnWebviewMessage(serialized_request, std::nullopt);
    run_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
    return upload_types;
  }

  // Returns the attached tabs of the session handle.
  const std::vector<contextual_search::TabInfo>& attached() {
    return mock_session_handle_->GetTabContextState().attached;
  }

 protected:
  base::test::ScopedFeatureList deselection_feature_list_;
  raw_ptr<tabs::TabInterface> active_tab_ = nullptr;
  SessionID active_session_id_ = SessionID::InvalidValue();
  GURL active_url_;
  std::string active_title_;
  raw_ptr<MockTabContextualizationController> mock_tab_controller_ = nullptr;
  NiceMock<MockContextualTasksService> mock_tasks_service_;
  base::Uuid task_id_;
  std::unique_ptr<base::WeakPtrFactory<
      contextual_search::ContextualSearchContextController>>
      controller_weak_factory_;
  base::ObserverList<contextual_search::ContextualSearchContextController::
                         ContextUploadStatusObserver>
      upload_observers_;
  std::unique_ptr<contextual_search::FileInfo> previous_upload_;
  // Upload types of the uploaded context tokens.
  std::map<base::UnguessableToken, UploadType> upload_types_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerContextualizerBrowserTest,
                       AddTabContext_DelayedTabRecordedWithoutCapture) {
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(_)).Times(0);
  EXPECT_CALL(*mock_session_handle_, CreateContextToken()).Times(0);

  const base::UnguessableToken chip_token = AddActiveTabChip();

  ASSERT_EQ(attached().size(), 1u);
  EXPECT_EQ(attached()[0].tab_id, active_session_id_.id());
  // The session handle holds the chip token.
  EXPECT_EQ(attached()[0].context_token, chip_token);
  EXPECT_TRUE(attached()[0].pending_delayed_upload);
  EXPECT_TRUE(mock_session_handle_->GetUploadedContextTokens().empty());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerContextualizerBrowserTest,
                       DeleteContext_DelayedTabRemoved) {
  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteContext(
      AddActiveTabChip(), /*from_automatic_chip=*/true);

  EXPECT_TRUE(attached().empty());
  EXPECT_CALL(*mock_session_handle_, StartTabContextUploadFlow(_, _, _))
      .Times(0);
  EXPECT_TRUE(Submit().empty());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerContextualizerBrowserTest,
                       OnSubmitQueryRequest_UploadsDelayedTab) {
  AddActiveTabChip();

  EXPECT_THAT(Submit(),
              testing::ElementsAre(
                  UploadType::CONTEXTUAL_INPUT_UPLOAD_TYPE_AUTO_TAB_CHIP));
  ASSERT_EQ(attached().size(), 1u);
  EXPECT_FALSE(attached()[0].context_token.is_empty());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerContextualizerBrowserTest,
                       OnSubmitQueryRequest_NoTaskContextStillPosts) {
  SeedUploadedActiveTab({1, 2, 3});
  SetActiveTabPageContext({4, 5, 6});
  // E.g. the task was deleted.
  EXPECT_CALL(mock_tasks_service_, GetContextForTask(task_id_, _, _, _))
      .WillOnce([](auto, auto, auto, auto callback) {
        std::move(callback).Run(nullptr);
      });
  EXPECT_CALL(*mock_session_handle_, StartTabContextUploadFlow(_, _, _))
      .Times(0);

  EXPECT_THAT(Submit(), testing::ElementsAre(
                            UploadType::CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerContextualizerBrowserTest,
                       OnSubmitQueryRequest_ReuploadsChangedActiveTab) {
  const base::UnguessableToken old_token = SeedUploadedActiveTab({1, 2, 3});
  SetActiveTabPageContext({4, 5, 6});

  // The response contains the re-upload instead of the superseded upload.
  EXPECT_THAT(
      Submit(),
      testing::ElementsAre(
          UploadType::CONTEXTUAL_INPUT_UPLOAD_TYPE_RECONTEXTUALIZATION));
  ASSERT_EQ(attached().size(), 1u);
  EXPECT_NE(attached()[0].context_token, old_token);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerContextualizerBrowserTest,
                       OnSubmitQueryRequest_ReuploadsWithHelperTaskId) {
  // Like the side panel: only the helper has the task id.
  const base::UnguessableToken old_token =
      SeedUploadedActiveTab({1, 2, 3}, /*task_id_from_helper=*/true);
  auto* user_data =
      contextual_tasks::ContextualTasksWebContentsUserData::FromWebContents(
          web_contents_);
  ASSERT_FALSE(user_data->task_id().has_value());
  EXPECT_EQ(user_data->GetEffectiveTaskId(), task_id_);
  SetActiveTabPageContext({4, 5, 6});

  EXPECT_THAT(
      Submit(),
      testing::ElementsAre(
          UploadType::CONTEXTUAL_INPUT_UPLOAD_TYPE_RECONTEXTUALIZATION));
  ASSERT_EQ(attached().size(), 1u);
  EXPECT_NE(attached()[0].context_token, old_token);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerContextualizerBrowserTest,
                       OnSubmitQueryRequest_DoesNotReuploadUnchangedActiveTab) {
  const base::UnguessableToken old_token = SeedUploadedActiveTab({1, 2, 3});
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(_));
  EXPECT_CALL(*mock_session_handle_, StartTabContextUploadFlow(_, _, _))
      .Times(0);

  EXPECT_THAT(Submit(), testing::ElementsAre(
                            UploadType::CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT));
  ASSERT_EQ(attached().size(), 1u);
  EXPECT_EQ(attached()[0].context_token, old_token);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextualizerBrowserTest,
    OnSubmitQueryRequest_DoesNotReuploadDeselectedActiveTab) {
  ASSERT_TRUE(omnibox::IsTabDeselectionInComposeboxEnabled());
  SeedUploadedActiveTab({1, 2, 3});
  SetActiveTabPageContext({4, 5, 6});
  mock_session_handle_->set_deselected_tabs_urls(
      {{active_session_id_, {active_url_, active_title_}}});
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(_)).Times(0);
  EXPECT_CALL(*mock_session_handle_, StartTabContextUploadFlow(_, _, _))
      .Times(0);

  EXPECT_THAT(Submit(), testing::ElementsAre(
                            UploadType::CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerContextualizerBrowserTest,
                       DeleteContext_SupersededTokenRemovesTab) {
  const base::UnguessableToken old_token = SeedUploadedActiveTab({1, 2, 3});
  SetActiveTabPageContext({4, 5, 6});
  Submit();
  ASSERT_EQ(attached().size(), 1u);
  ASSERT_NE(attached()[0].context_token, old_token);

  // The frontend still has the token of the superseded upload.
  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteContext(
      old_token, /*from_automatic_chip=*/false);

  EXPECT_TRUE(attached().empty());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerContextualizerBrowserTest,
                       DeleteContext_DelayedTabTokenAfterSubmitRemovesTab) {
  const base::UnguessableToken chip_token = AddActiveTabChip();
  Submit();
  ASSERT_EQ(attached().size(), 1u);
  ASSERT_NE(attached()[0].context_token, chip_token);

  // The frontend still has the delayed tab's chip token.
  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteContext(
      chip_token, /*from_automatic_chip=*/true);

  EXPECT_TRUE(attached().empty());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       DeleteTabContext_ClosedTabRemovesUnderline) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com/second_tab"),
      WindowOpenDisposition::kNewForegroundTab,
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

  handler_->OnWebviewMessage(serialized_message, std::nullopt);
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

  handler_->OnWebviewMessage(serialized_message, std::nullopt);
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
  bool received_context_state_none = false;
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
        } else if (message->has_on_context_state_changed()) {
          EXPECT_EQ(message->on_context_state_changed().context_state(),
                    lens::ClientToSearchMessage::OnContextStateChanged::
                        CONTEXT_STATE_NONE);
          received_context_state_none = true;
        }
        if (received_submit_response && received_lens_chip_unmount &&
            received_context_state_none) {
          run_loop.Quit();
        }
      });

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message, std::nullopt);
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
  bool received_context_state_none = false;
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
        } else if (message->has_on_context_state_changed()) {
          EXPECT_EQ(message->on_context_state_changed().context_state(),
                    lens::ClientToSearchMessage::OnContextStateChanged::
                        CONTEXT_STATE_NONE);
          received_context_state_none = true;
        }
        if (received_submit_response && received_lens_chip_unmount &&
            received_context_state_none) {
          run_loop.Quit();
        }
      });

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message, std::nullopt);
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
  handler_->OnWebviewMessage(serialized_message, std::nullopt);
  run_loop.Run();
  mock_page_.FlushForTesting();
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
  handler_->OnWebviewMessage(serialized_message, std::nullopt);
  run_loop.Run();
  mock_page_.FlushForTesting();
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
  handler_->OnWebviewMessage(serialized_message, std::nullopt);
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

  handler_->OnWebviewMessage(serialized_message, std::nullopt);
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

  handler_->OnWebviewMessage(serialized_message, std::nullopt);
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
  handler_->OnWebviewMessage(serialized_message, std::nullopt);
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

  handler_->OnWebviewMessage(serialized_message, std::nullopt);
  run_loop2.Run();
  mock_page_.FlushForTesting();
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
      WindowOpenDisposition::kNewForegroundTab,
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
      browser(), GURL("about:blank"), WindowOpenDisposition::kNewForegroundTab,
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
  mock_page_.FlushForTesting();
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
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        // The submitted tab is still shown by the context library, so it stays
        // mounted while context state resets to CONTEXT_STATE_NONE.
        auto client_message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(client_message.has_value());
        ASSERT_TRUE(client_message->has_on_context_state_changed());
        EXPECT_EQ(client_message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
        run_loop.Quit();
      });

  lens::SearchToClientMessage search_message;
  search_message.mutable_on_submit_query_request();
  const size_t size = search_message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  search_message.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message, std::nullopt);
  run_loop.Run();
  mock_page_.FlushForTesting();
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
      WindowOpenDisposition::kNewForegroundTab,
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
  mock_page_.FlushForTesting();
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
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        // The previous turn's tab is still shown by the context library, so it
        // stays mounted while context state resets to CONTEXT_STATE_NONE.
        auto client_message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(client_message.has_value());
        ASSERT_TRUE(client_message->has_on_context_state_changed());
        EXPECT_EQ(client_message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
        run_loop.Quit();
      });

  lens::SearchToClientMessage search_message;
  search_message.mutable_on_submit_query_request();
  const size_t size = search_message.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  search_message.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message, std::nullopt);
  run_loop.Run();
  mock_page_.FlushForTesting();
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
  mock_session_handle_->SetPersistedTabs(
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
        handler_->OnWebviewMessage(serialized_message, std::nullopt);
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
      mock_session_handle_->GetPersistedTabs().contains(deselected_session_id));

  // Second turn: nothing new was uploaded or removed.
  submit(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage::OnSubmitQueryResponse& response) {
        EXPECT_EQ(0, response.added_contexts_size());
        EXPECT_EQ(0, response.removed_contexts_size());
      }));
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
  ui_service->OnLensOverlayStateChanged(
      browser(), true,
      lens::LensOverlayInvocationSource::kContextualTasksComposebox);
  mock_page_.FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    ContextualTasksUiService_OnLensOverlayStateChanged_OnlyActiveForComposeboxSource) {
  EmbedHandlerInSidePanel();
  ContextualTasksPanelController::From(browser())->Show(
      /*transition_from_tab=*/false,
      omnibox::ChromeAimEntryPoint::UNKNOWN_AIM_ENTRY_POINT,
      /*use_no_animation=*/true);

  auto* ui_service = ContextualTasksUiServiceFactory::GetForBrowserContext(
      browser()->GetProfile());
  ASSERT_TRUE(ui_service);

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_panel_page_, OnLensOverlayStateChanged(false))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
    ui_service->OnLensOverlayStateChanged(
        browser(), true, lens::LensOverlayInvocationSource::kAppMenu);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_panel_page_, OnLensOverlayStateChanged(true))
        .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
    ui_service->OnLensOverlayStateChanged(
        browser(), true,
        lens::LensOverlayInvocationSource::kContextualTasksComposebox);
    run_loop.Run();
  }
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

  handler_->OnWebviewMessage(serialized_message, std::nullopt);

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
    handler_->OnWebviewMessage(serialized_message, std::nullopt);
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
    handler_->OnWebviewMessage(serialized_message, std::nullopt);
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

  handler_->OnWebviewMessage(serialized_message, std::nullopt);

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
    ContextualTasksExtensionHandlerClobberActiveTabBrowserTest,
    OnWebviewMessage_SearchToClientMessageOpenLinkInSidePanelMode_ClobbersActiveTab) {
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

  handler_->OnWebviewMessage(serialized_message, std::nullopt);

  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(active_contents, nullptr);
  EXPECT_EQ(active_contents, original_tab_contents);
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
    ContextualTasksExtensionHandlerClobberActiveTabBrowserTest,
    OnWebviewMessage_AimToClientMessageOpenLinkInSidePanelMode_ClobbersActiveTab) {
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

  handler_->OnWebviewMessage(serialized_message, std::nullopt);

  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);
  content::WebContents* active_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_NE(active_contents, nullptr);
  EXPECT_EQ(active_contents, original_tab_contents);
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

  mock_session_handle_->AttachTabPendingUpload(
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
    OnTabContextStateChanged_ResolvesSessionTabIdToTabHandleId) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  const int32_t active_tab_handle_id = active_tab->GetHandle().raw_value();

  base::RunLoop notify_run_loop;
  EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
      .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                    const std::vector<int32_t>& submitted_tab_ids) {
        ASSERT_EQ(tabs.size(), 1u);
        EXPECT_EQ(tabs[0]->tab_id, active_tab_handle_id);
        notify_run_loop.Quit();
      });

  base::RunLoop add_run_loop;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      active_tab_handle_id, /*delay_upload=*/true,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            add_run_loop.Quit();
          }));
  add_run_loop.Run();
  notify_run_loop.Run();
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
  mock_session_handle_->AttachTabPendingUpload(
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

  mock_session_handle_->AttachTabPendingUpload(
      unsubmitted_token, 11, GURL("https://example.com/active"), "Active Tab");
  mock_session_handle_->AttachTabPendingUpload(
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
        EXPECT_EQ(tabs[0]->tab_id, 12);
        EXPECT_EQ(tabs[1]->tab_id, 11);
        EXPECT_EQ(tabs[2]->tab_id, 13);
        EXPECT_THAT(submitted_tab_ids, testing::ElementsAre(12, 13));
        run_loop.Quit();
      });

  mojo::PendingReceiver<mojom::ExtensionPageHandler> child_handler_receiver;
  child_handler->CreateExtensionPageHandler(mock_child_page.BindAndGetRemote(),
                                            std::move(child_handler_receiver));
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       OnTabContextStateChanged_OrdersTabsMostRecentFirst) {
  auto attach_tab = [&](int32_t tab_id, const std::string& url,
                        const std::string& title,
                        std::vector<int32_t> expected_order) {
    const base::UnguessableToken token = base::UnguessableToken::Create();
    mock_session_handle_->GetUploadedContextTokensForTesting().push_back(token);

    base::RunLoop run_loop;
    EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
        .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                      const std::vector<int32_t>& /*submitted_tab_ids*/) {
          std::vector<int32_t> actual_ids;
          for (const auto& tab : tabs) {
            actual_ids.push_back(tab->tab_id);
          }
          EXPECT_EQ(actual_ids, expected_order);
          run_loop.Quit();
        });

    mock_session_handle_->AttachTabPendingUpload(token, tab_id, GURL(url),
                                                 title);
    run_loop.Run();
  };

  attach_tab(1, "https://example.com/1", "Tab 1", {1});
  attach_tab(2, "https://example.com/2", "Tab 2", {2, 1});
  attach_tab(3, "https://example.com/3", "Tab 3", {3, 2, 1});
  attach_tab(4, "https://example.com/4", "Tab 4", {4, 3, 2, 1});

  contextual_search::TabInfo restored_1;
  restored_1.tab_id = 10;
  restored_1.url = GURL("https://example.com/r1");
  restored_1.title = "Restored 1";

  contextual_search::TabInfo restored_2;
  restored_2.tab_id = 20;
  restored_2.url = GURL("https://example.com/r2");
  restored_2.title = "Restored 2";

  base::RunLoop restore_run_loop;
  EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
      .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                    const std::vector<int32_t>& submitted_tab_ids) {
        std::vector<int32_t> actual_ids;
        for (const auto& tab : tabs) {
          actual_ids.push_back(tab->tab_id);
        }
        EXPECT_THAT(actual_ids, testing::ElementsAre(4, 3, 2, 1, 20, 10));
        EXPECT_THAT(submitted_tab_ids, testing::ElementsAre(20, 10));
        restore_run_loop.Quit();
      });

  mock_session_handle_->SetRestoredTabs({restored_1, restored_2});
  restore_run_loop.Run();
}

class ContextualTasksExtensionHandlerContextLibraryBrowserTest
    : public ContextualTasksExtensionHandlerBrowserTestBase {
 public:
  ContextualTasksExtensionHandlerContextLibraryBrowserTest()
      : ContextualTasksExtensionHandlerBrowserTestBase(
            {kContextualTasks, kContextualTasksRearchitecture,
             kContextualTasksForceEntryPointEligibility,
             kContextualTasksContextLibrary},
            {}) {
    tab_deselection_feature_list_.InitAndEnableFeatureWithParameters(
        omnibox::kContextManagementInComposebox,
        {{"enable_tab_deselection", "true"}});
  }

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
    handler_->OnWebviewMessage(serialized_message, std::nullopt);
  }

  // Opens `url` in a background tab, so `web_contents_` stays active, and
  // returns the new tab's session ID. Restored tabs are only kept while their
  // tab is open.
  SessionID OpenBackgroundTab(const GURL& url) {
    EXPECT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::kNewBackgroundTab,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
    for (tabs::TabInterface* tab :
         TabListInterface::From(browser())->GetAllTabs()) {
      if (tab->GetContents()->GetLastCommittedURL() == url) {
        return sessions::SessionTabHelper::IdForTab(tab->GetContents());
      }
    }
    ADD_FAILURE() << "No open tab for " << url;
    return SessionID::InvalidValue();
  }

  // Closes the tab with `session_id` and waits for it to be destroyed.
  void CloseTab(SessionID session_id) {
    for (tabs::TabInterface* tab :
         TabListInterface::From(browser())->GetAllTabs()) {
      if (sessions::SessionTabHelper::IdForTab(tab->GetContents()) ==
          session_id) {
        content::WebContentsDestroyedWatcher destroyed_watcher(
            tab->GetContents());
        TabListInterface::From(browser())->CloseTab(tab->GetHandle());
        destroyed_watcher.Wait();
        return;
      }
    }
    ADD_FAILURE() << "No open tab for session ID " << session_id.id();
  }

  // Returns the TabHandle ID, as used by the frontend, of the tab with
  // `session_id`.
  int32_t GetTabHandleId(SessionID session_id) {
    for (tabs::TabInterface* tab :
         TabListInterface::From(browser())->GetAllTabs()) {
      if (sessions::SessionTabHelper::IdForTab(tab->GetContents()) ==
          session_id) {
        return tab->GetHandle().raw_value();
      }
    }
    ADD_FAILURE() << "No open tab for session ID " << session_id.id();
    return -1;
  }

 private:
  base::test::ScopedFeatureList tab_deselection_feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerContextLibraryBrowserTest,
                       DeleteTabContext_HidesRestoredTabOnLaterThreadUpdates) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url = embedded_test_server()->GetURL("/title1.html");
  const SessionID restored_tab = OpenBackgroundTab(url);
  handler_->SetTaskId(base::Uuid::GenerateRandomV4());

  lens::SearchToClientMessage::UpdateThreadContextLibrary library;
  AddWebpageContext(library, 1, url.spec(), "Restored Tab");
  SendUpdateThreadContextLibrary(library);
  ASSERT_EQ(mock_session_handle_->GetTabContextState().restored.size(), 1u);

  // Deselecting a restored tab hides it.
  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      GetTabHandleId(restored_tab));
  EXPECT_TRUE(mock_session_handle_->GetTabContextState().restored.empty());

  // A later thread update from AIM keeps the deselected tab hidden.
  SendUpdateThreadContextLibrary(library);
  EXPECT_TRUE(mock_session_handle_->GetTabContextState().restored.empty());
}

// Enables deselecting tabs that were submitted in an earlier turn.
class ContextualTasksExtensionHandlerTabDeselectionBrowserTest
    : public ContextualTasksExtensionHandlerBrowserTestBase {
 public:
  ContextualTasksExtensionHandlerTabDeselectionBrowserTest()
      : ContextualTasksExtensionHandlerBrowserTestBase(
            {kContextualTasks, kContextualTasksRearchitecture,
             kContextualTasksForceEntryPointEligibility},
            {}) {
    tab_deselection_feature_list_.InitAndEnableFeatureWithParameters(
        omnibox::kContextManagementInComposebox,
        {{"enable_tab_deselection", "true"}});
  }

 protected:
  // Persists the active tab as if it was submitted in an earlier turn with
  // `request_id`, and returns its TabHandle ID.
  int32_t PersistActiveTab(const base::UnguessableToken& token,
                           const lens::LensOverlayRequestId& request_id) {
    const SessionID session_id =
        sessions::SessionTabHelper::IdForTab(web_contents_);
    persisted_file_info_.file_token = token;
    persisted_file_info_.tab_session_id = session_id;
    persisted_file_info_.tab_url = web_contents_->GetLastCommittedURL();
    persisted_file_info_.request_id = request_id;
    ON_CALL(*mock_controller_, GetFileInfo(token))
        .WillByDefault(Return(&persisted_file_info_));
    mock_session_handle_->SetPersistedTabs({{session_id, {token, request_id}}});
    return TabListInterface::From(browser())
        ->GetActiveTab()
        ->GetHandle()
        .raw_value();
  }

 private:
  base::test::ScopedFeatureList tab_deselection_feature_list_;
  contextual_search::FileInfo persisted_file_info_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerTabDeselectionBrowserTest,
                       DeleteTabContext_DeselectsSubmittedTab) {
  lens::LensOverlayRequestId request_id;
  request_id.set_uuid(4242);
  const int32_t tab_handle_id =
      PersistActiveTab(base::UnguessableToken::Create(), request_id);
  ASSERT_EQ(mock_session_handle_->GetTabContextState().attached.size(), 1u);

  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      tab_handle_id);

  EXPECT_TRUE(mock_session_handle_->GetTabContextState().attached.empty());
  // The next submission tells the server to drop the deselected tab.
  std::vector<lens::LensOverlayRequestId> removed_contexts =
      mock_session_handle_->TakeRemovedContexts();
  ASSERT_EQ(removed_contexts.size(), 1u);
  EXPECT_EQ(removed_contexts[0].uuid(), 4242u);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerTabDeselectionBrowserTest,
                       DeleteTabContext_HidesRestoredCopyOfSubmittedTab) {
  lens::LensOverlayRequestId request_id;
  request_id.set_uuid(4242);
  const int32_t tab_handle_id =
      PersistActiveTab(base::UnguessableToken::Create(), request_id);
  // After a turn, AIM's thread update also restores the submitted tab.
  contextual_search::TabInfo restored_tab;
  restored_tab.tab_id =
      sessions::SessionTabHelper::IdForTab(web_contents_).id();
  restored_tab.url = web_contents_->GetLastCommittedURL();
  restored_tab.submitted = true;
  mock_session_handle_->SetRestoredTabs({restored_tab});
  ASSERT_EQ(mock_session_handle_->GetTabContextState().attached.size(), 1u);
  ASSERT_EQ(mock_session_handle_->GetTabContextState().restored.size(), 1u);

  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      tab_handle_id);

  // Both copies are removed, so the tab no longer shows as selected.
  EXPECT_TRUE(mock_session_handle_->GetTabContextState().attached.empty());
  EXPECT_TRUE(mock_session_handle_->GetTabContextState().restored.empty());
  // The next submission still tells the server to drop the deselected tab.
  std::vector<lens::LensOverlayRequestId> removed_contexts =
      mock_session_handle_->TakeRemovedContexts();
  ASSERT_EQ(removed_contexts.size(), 1u);
  EXPECT_EQ(removed_contexts[0].uuid(), 4242u);
}

// Disables tab deselection, which the field trial testing config and the
// Contextual Tasks rearchitecture flags enable.
class ContextualTasksExtensionHandlerNoTabDeselectionBrowserTest
    : public ContextualTasksExtensionHandlerBrowserTestBase {
 public:
  ContextualTasksExtensionHandlerNoTabDeselectionBrowserTest()
      : ContextualTasksExtensionHandlerBrowserTestBase(
            {kContextualTasks, kContextualTasksForceEntryPointEligibility},
            {kContextualTasksRearchitecture,
             kContextualTasksSidePanelRearchitecture}) {
    tab_deselection_feature_list_.InitAndEnableFeatureWithParameters(
        omnibox::kContextManagementInComposebox,
        {{"enable_tab_deselection", "false"}});
  }

 private:
  base::test::ScopedFeatureList tab_deselection_feature_list_;
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerNoTabDeselectionBrowserTest,
    DeleteTabContext_KeepsSubmittedTabWithoutTabDeselection) {
  const SessionID session_id =
      sessions::SessionTabHelper::IdForTab(web_contents_);
  const base::UnguessableToken token = base::UnguessableToken::Create();
  lens::LensOverlayRequestId request_id;
  request_id.set_uuid(4242);
  contextual_search::FileInfo file_info;
  file_info.file_token = token;
  file_info.tab_session_id = session_id;
  file_info.request_id = request_id;
  ON_CALL(*mock_controller_, GetFileInfo(token))
      .WillByDefault(Return(&file_info));
  mock_session_handle_->SetPersistedTabs({{session_id, {token, request_id}}});

  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      TabListInterface::From(browser())
          ->GetActiveTab()
          ->GetHandle()
          .raw_value());

  // Without tab deselection the server keeps submitted tabs, so they stay
  // attached instead of disappearing from the UI only.
  EXPECT_EQ(mock_session_handle_->GetTabContextState().attached.size(), 1u);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextLibraryBrowserTest,
    OnWebviewMessage_UpdateThreadContextLibrary_SetsRestoredTabsWhenEmpty) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a = embedded_test_server()->GetURL("/title1.html");
  const GURL url_b = embedded_test_server()->GetURL("/title2.html");
  const SessionID tab_a = OpenBackgroundTab(url_a);
  const SessionID tab_b = OpenBackgroundTab(url_b);
  handler_->SetTaskId(base::Uuid::GenerateRandomV4());
  ASSERT_TRUE(mock_session_handle_->GetTabContextState().restored.empty());

  lens::SearchToClientMessage::UpdateThreadContextLibrary library;
  AddWebpageContext(library, 1, url_a.spec(), "Tab A");
  AddWebpageContext(library, 2, url_b.spec(), "Tab B");
  // Non-tab contexts should not be added as restored tabs.
  auto* pdf_context = library.add_contexts();
  pdf_context->set_context_id(3);
  pdf_context->mutable_pdf()->set_url("https://example.com/doc.pdf");
  pdf_context->mutable_pdf()->set_title("Doc");

  SendUpdateThreadContextLibrary(library);

  // Restored tabs are resolved to the open tabs showing their URLs.
  const auto& restored = mock_session_handle_->GetTabContextState().restored;
  ASSERT_EQ(restored.size(), 2u);
  EXPECT_EQ(restored[0].tab_id, tab_a.id());
  EXPECT_EQ(restored[0].url, url_a);
  EXPECT_EQ(restored[0].title, "Tab A");
  EXPECT_TRUE(restored[0].restored_from_aim);
  EXPECT_TRUE(restored[0].submitted);
  EXPECT_EQ(restored[1].tab_id, tab_b.id());
  EXPECT_EQ(restored[1].url, url_b);
  EXPECT_EQ(restored[1].title, "Tab B");
  EXPECT_TRUE(restored[1].restored_from_aim);
  EXPECT_TRUE(restored[1].submitted);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextLibraryBrowserTest,
    OnWebviewMessage_UpdateThreadContextLibrary_ReplacesOldRestoredTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a = embedded_test_server()->GetURL("/title1.html");
  const GURL url_b = embedded_test_server()->GetURL("/title2.html");
  const GURL url_c = embedded_test_server()->GetURL("/title3.html");
  const SessionID tab_a = OpenBackgroundTab(url_a);
  OpenBackgroundTab(url_b);
  const SessionID tab_c = OpenBackgroundTab(url_c);
  handler_->SetTaskId(base::Uuid::GenerateRandomV4());

  lens::SearchToClientMessage::UpdateThreadContextLibrary first_library;
  AddWebpageContext(first_library, 1, url_a.spec(), "Tab A");
  AddWebpageContext(first_library, 2, url_b.spec(), "Tab B");
  SendUpdateThreadContextLibrary(first_library);
  ASSERT_EQ(mock_session_handle_->GetTabContextState().restored.size(), 2u);

  // A later restored tabs update replaces the restored tabs with the server's
  // latest list, in server order.
  lens::SearchToClientMessage::UpdateThreadContextLibrary second_library;
  AddWebpageContext(second_library, 3, url_c.spec(), "Tab C");
  AddWebpageContext(second_library, 1, url_a.spec(), "Tab A");
  SendUpdateThreadContextLibrary(second_library);

  const auto& restored = mock_session_handle_->GetTabContextState().restored;
  ASSERT_EQ(restored.size(), 2u);
  EXPECT_EQ(restored[0].tab_id, tab_c.id());
  EXPECT_EQ(restored[0].url, url_c);
  EXPECT_EQ(restored[0].title, "Tab C");
  EXPECT_EQ(restored[1].tab_id, tab_a.id());
  EXPECT_EQ(restored[1].url, url_a);
  EXPECT_EQ(restored[1].title, "Tab A");
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextLibraryBrowserTest,
    OnWebviewMessage_UpdateThreadContextLibrary_KeepsOpenTabsAndDropsClosedTabs) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL open_url = embedded_test_server()->GetURL("/title1.html");
  const GURL submitted_url = embedded_test_server()->GetURL("/title2.html");
  const SessionID open_tab = OpenBackgroundTab(open_url);
  const SessionID submitted_tab = OpenBackgroundTab(submitted_url);
  handler_->SetTaskId(base::Uuid::GenerateRandomV4());

  // The first update matches context 2 to the tab submitted in this session,
  // even though that tab no longer shows the server's URL. Later updates find
  // no submitted context, since its tokens get cleared.
  base::UnguessableToken submitted_token = base::UnguessableToken::Create();
  mock_session_handle_->SetSubmittedContextTokens({submitted_token});
  contextual_search::FileInfo submitted_file;
  submitted_file.file_token = submitted_token;
  submitted_file.tab_session_id = submitted_tab;
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(2);
  submitted_file.request_id = req_id;
  EXPECT_CALL(*mock_session_handle_, GetSubmittedContextFileInfos())
      .WillOnce(
          Return(std::vector<contextual_search::FileInfo>{submitted_file}))
      .WillRepeatedly(Return(std::vector<contextual_search::FileInfo>()));

  lens::SearchToClientMessage::UpdateThreadContextLibrary library;
  // Matched to an open tab by URL.
  AddWebpageContext(library, 1, open_url.spec(), "Open Tab");
  // Matched to the submitted tab, whose URL no longer matches the server's.
  AddWebpageContext(library, 2, "https://example.com/navigated", "Submitted");
  // Not open in any tab.
  AddWebpageContext(library, 3, "https://example.com/closed", "Closed Tab");
  SendUpdateThreadContextLibrary(library);
  {
    const auto& restored = mock_session_handle_->GetTabContextState().restored;
    ASSERT_EQ(restored.size(), 2u);
    EXPECT_EQ(restored[0].tab_id, open_tab.id());
    EXPECT_EQ(restored[1].tab_id, submitted_tab.id());
  }

  // Later updates only match tabs that still show the page, so the submitted
  // tab, which no longer does, is dropped.
  SendUpdateThreadContextLibrary(library);
  {
    const auto& restored = mock_session_handle_->GetTabContextState().restored;
    ASSERT_EQ(restored.size(), 1u);
    EXPECT_EQ(restored[0].tab_id, open_tab.id());
  }

  // Once its tab is closed, the next update drops the restored tab.
  CloseTab(open_tab);
  SendUpdateThreadContextLibrary(library);
  EXPECT_TRUE(mock_session_handle_->GetTabContextState().restored.empty());
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
  mock_session_handle_->SetSubmittedContextTokens({submitted_token});

  contextual_search::FileInfo submitted_file;
  submitted_file.file_token = submitted_token;
  submitted_file.tab_session_id = active_tab_id;
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(101);
  submitted_file.request_id = req_id;
  EXPECT_CALL(*mock_session_handle_, GetSubmittedContextFileInfos())
      .WillRepeatedly(
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
  EXPECT_EQ(restored[0].tab_id, active_tab_id.id());
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
  EXPECT_EQ(resources[0].tab_id, active_tab_id);
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
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a = embedded_test_server()->GetURL("/title1.html");
  OpenBackgroundTab(url_a);

  // Seed existing restored tabs and submitted tokens.
  lens::SearchToClientMessage::UpdateThreadContextLibrary initial_library;
  AddWebpageContext(initial_library, 1, url_a.spec(), "Tab A");
  SendUpdateThreadContextLibrary(initial_library);
  ASSERT_EQ(mock_session_handle_->GetTabContextState().restored.size(), 1u);

  base::UnguessableToken submitted_token = base::UnguessableToken::Create();
  mock_session_handle_->SetSubmittedContextTokens({submitted_token});

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
    OnWebviewMessage_UpdateThreadContextLibrary_UsesHelperTaskId) {
  auto* tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(tasks_service, nullptr);
  ContextualTask task = tasks_service->CreateTask();
  SessionID active_tab_id = sessions::SessionTabHelper::IdForTab(web_contents_);
  tasks_service->AssociateTabWithTask(task.GetTaskId(), active_tab_id);
  // Like the side panel: only the helper has the task id.
  ASSERT_TRUE(ContextualSearchWebContentsHelper::FromWebContents(web_contents_)
                  ->GetSessionForTask(task.GetTaskId()));
  // Restored tabs are only kept while their tab is open.
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a = embedded_test_server()->GetURL("/title1.html");
  OpenBackgroundTab(url_a);

  lens::SearchToClientMessage::UpdateThreadContextLibrary library;
  AddWebpageContext(library, 1, url_a.spec(), "Tab A");
  SendUpdateThreadContextLibrary(library);

  ASSERT_EQ(mock_session_handle_->GetTabContextState().restored.size(), 1u);
  std::optional<ContextualTask> updated_task =
      tasks_service->GetContextualTaskForTab(active_tab_id);
  ASSERT_TRUE(updated_task.has_value());
  ASSERT_EQ(updated_task->GetUrlResources().size(), 1u);
  EXPECT_EQ(updated_task->GetUrlResources()[0].url, url_a);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextLibraryBrowserTest,
    OnWebviewMessage_UpdateThreadContextLibrary_WithoutTaskIdStillRestoresTabs) {
  auto* tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(tasks_service, nullptr);
  ContextualTask task = tasks_service->CreateTask();
  SessionID active_tab_id = sessions::SessionTabHelper::IdForTab(web_contents_);
  tasks_service->AssociateTabWithTask(task.GetTaskId(), active_tab_id);
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url_a = embedded_test_server()->GetURL("/title1.html");
  const SessionID tab_a = OpenBackgroundTab(url_a);

  base::UnguessableToken submitted_token = base::UnguessableToken::Create();
  mock_session_handle_->SetSubmittedContextTokens({submitted_token});

  lens::SearchToClientMessage::UpdateThreadContextLibrary library;
  AddWebpageContext(library, 1, url_a.spec(), "Tab A");

  // Before a task ID is set (e.g. full-tab AI Mode before `mtid` is in the
  // URL), the session handle's restored tabs are still updated and the context
  // library mounts, while `ContextualTasksService` is untouched.
  base::RunLoop mount_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_inject_chrome_input());
        EXPECT_EQ(
            message->inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
        EXPECT_TRUE(message->inject_chrome_input().is_active());
        mount_run_loop.Quit();
      });
  SendUpdateThreadContextLibrary(library);
  mount_run_loop.Run();

  const auto& restored = mock_session_handle_->GetTabContextState().restored;
  ASSERT_EQ(restored.size(), 1u);
  EXPECT_EQ(restored[0].tab_id, tab_a.id());
  EXPECT_TRUE(mock_session_handle_->GetSubmittedContextTokens().empty());
  std::optional<ContextualTask> updated_task =
      tasks_service->GetContextualTaskForTab(active_tab_id);
  ASSERT_TRUE(updated_task.has_value());
  EXPECT_TRUE(updated_task->GetUrlResources().empty());

  // With task_id set but no session handle in the helper, the update returns
  // early without updating ContextualTasksService.
  handler_->SetTaskId(task.GetTaskId());
  mock_session_handle_ = nullptr;
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
      ->SetTaskSession(task.GetTaskId(), /*handle=*/nullptr,
                       /*input_state_model=*/nullptr);

  SendUpdateThreadContextLibrary(library);
  updated_task = tasks_service->GetContextualTaskForTab(active_tab_id);
  ASSERT_TRUE(updated_task.has_value());
  EXPECT_TRUE(updated_task->GetUrlResources().empty());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerContextLibraryBrowserTest,
    OnWebviewMessage_UpdateThreadContextLibrary_UsesHelperTaskIdAndMountsLibrary) {
  auto* tasks_service =
      ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile());
  ASSERT_NE(tasks_service, nullptr);
  ContextualTask task = tasks_service->CreateTask();
  // Extension frames never call `SetTaskId()`, so the task is only tracked by
  // the WebContents helper, which adopts the untasked session here.
  ASSERT_EQ(ContextualSearchWebContentsHelper::FromWebContents(web_contents_)
                ->GetSessionForTask(task.GetTaskId()),
            mock_session_handle_);

  base::UnguessableToken submitted_token = base::UnguessableToken::Create();
  mock_session_handle_->SetSubmittedContextTokens({submitted_token});
  const SessionID active_tab_id =
      sessions::SessionTabHelper::IdForTab(web_contents_);
  contextual_search::FileInfo submitted_file;
  submitted_file.file_token = submitted_token;
  submitted_file.tab_session_id = active_tab_id;
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(101);
  submitted_file.request_id = req_id;
  EXPECT_CALL(*mock_session_handle_, GetSubmittedContextFileInfos())
      .WillOnce(
          Return(std::vector<contextual_search::FileInfo>{submitted_file}));

  // Restoring a thread whose only tabs come from AIM mounts the library.
  base::RunLoop mount_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_inject_chrome_input());
        EXPECT_EQ(
            message->inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
        EXPECT_TRUE(message->inject_chrome_input().is_active());
        mount_run_loop.Quit();
      });

  lens::SearchToClientMessage::UpdateThreadContextLibrary library;
  AddWebpageContext(library, 101, "https://example.com/a", "Tab A");
  SendUpdateThreadContextLibrary(library);
  mount_run_loop.Run();

  const auto& restored = mock_session_handle_->GetTabContextState().restored;
  ASSERT_EQ(restored.size(), 1u);
  EXPECT_EQ(restored[0].tab_id, active_tab_id.id());
  EXPECT_EQ(restored[0].url, GURL("https://example.com/a"));
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

  // Verify OnSubmitQueryRequest and CONTEXT_LIBRARY InjectChromeInput are
  // handled by ContextualTasksWebContentsUserData with 0 iframes.
  contextual_search::FileInfo file_info;
  file_info.file_token = base::UnguessableToken::Create();
  AttachActiveTab(file_info.file_token);
  ASSERT_EQ(dispatched_messages.size(), 2u);
  ASSERT_TRUE(dispatched_messages[0].has_inject_chrome_input());
  EXPECT_EQ(dispatched_messages[0].inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
  EXPECT_TRUE(dispatched_messages[0].inject_chrome_input().is_active());
  ASSERT_TRUE(dispatched_messages[1].has_on_context_state_changed());
  EXPECT_EQ(
      dispatched_messages[1].on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
  dispatched_messages.clear();

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
  ASSERT_EQ(dispatched_messages.size(), 2u);
  ASSERT_TRUE(dispatched_messages[0].has_on_submit_query_response());
  const auto& resp = dispatched_messages[0].on_submit_query_response();
  ASSERT_EQ(resp.added_contexts_size(), 1);
  EXPECT_EQ(resp.added_contexts(0).search_session_id(), "zero_iframe_session");
  EXPECT_EQ(resp.added_contexts(0).request_id().context_id(), 777);
  // The submitted tab is still shown by the context library, so it stays
  // mounted while context state resets to CONTEXT_STATE_NONE.
  ASSERT_TRUE(dispatched_messages[1].has_on_context_state_changed());
  EXPECT_EQ(
      dispatched_messages[1].on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_NONE);
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
  // emits CONTEXT_STATE_NONE (previous-turn context is excluded). The submitted
  // tab is still shown by the context library, so it stays mounted.
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
  handler_->OnWebviewMessage(serialized_message, std::nullopt);
  submit_run_loop.Run();
}

// A delayed tab is only uploaded on submit, so it is ready right away.
IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       OnContextStateChanged_DelayedTabReadyWithoutUpload) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t tab_id = active_tab->GetHandle().raw_value();

  EXPECT_CALL(*mock_session_handle_, StartTabContextUploadFlow(_, _, _))
      .Times(0);
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
  handler_->OnWebviewMessage(serialized_handshake, std::nullopt);
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

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    ServiceWorkerHandler_ConnectAndBidirectionalSearchMessage) {
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->UnregisterExtensionFrame(handler_);
  EXPECT_FALSE(user_data->HasBoundExtensionFrame());

  user_data->OnLensThumbnailCreated("data:image/png;base64,sw_mojo_crop");

  auto* bridge = ContextualTasksExtensionBridge::Get(GetProfile());
  ASSERT_NE(bridge, nullptr);

  mojo::Remote<mojom::ExtensionPageHandlerFactory> factory_remote;
  bridge->service_worker_handler().BindFactory(
      factory_remote.BindNewPipeAndPassReceiver());

  testing::NiceMock<MockContextualTasksExtensionPage> sw_page;
  mojo::Remote<mojom::ExtensionPageHandler> sw_handler;
  factory_remote->CreateExtensionPageHandler(
      sw_page.BindAndGetRemote(), sw_handler.BindNewPipeAndPassReceiver());

  std::string doc_id = extensions::ExtensionApiFrameIdMap::GetDocumentId(
                           web_contents_->GetPrimaryMainFrame())
                           .ToString();

  base::test::TestFuture<mojo_base::ProtoWrapper> handshake_future;
  sw_handler->GetHandshakeMessage(doc_id, handshake_future.GetCallback());
  mojo_base::ProtoWrapper handshake_wrapper = handshake_future.Take();
  auto ping_msg = handshake_wrapper.As<lens::ClientToAimMessage>();
  ASSERT_TRUE(ping_msg.has_value());
  EXPECT_TRUE(ping_msg->has_handshake_ping());
  EXPECT_TRUE(user_data->IsServiceWorkerPortConnected());

  EXPECT_CALL(*mock_controller_, SetAuthUserIndex(4));
  base::test::TestFuture<lens::ClientToSearchMessage,
                         std::optional<std::string>>
      inject_future;
  base::test::TestFuture<lens::ClientToSearchMessage,
                         std::optional<std::string>>
      state_future;
  EXPECT_CALL(sw_page, PostSearchMessageWithDocumentId(_, _))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper,
                    const std::optional<std::string>& received_doc_id) {
        auto parsed = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(parsed.has_value());
        inject_future.SetValue(*parsed, received_doc_id);
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper,
                    const std::optional<std::string>& received_doc_id) {
        auto parsed = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(parsed.has_value());
        state_future.SetValue(*parsed, received_doc_id);
      });

  lens::SearchToClientMessage handshake_resp;
  handshake_resp.mutable_handshake_response()->set_auth_user_index(4);
  std::string resp_str;
  ASSERT_TRUE(handshake_resp.SerializeToString(&resp_str));
  std::vector<uint8_t> resp_bytes(resp_str.begin(), resp_str.end());

  sw_handler->OnWebviewMessage(resp_bytes, doc_id);
  auto [inject_msg, received_doc_id] = inject_future.Take();
  auto [state_msg, state_doc_id] = state_future.Take();

  EXPECT_TRUE(user_data->IsHandshakeCompleteForTesting());
  EXPECT_EQ(mock_session_handle_->auth_user_index(), 4u);
  EXPECT_EQ(received_doc_id, doc_id);
  ASSERT_TRUE(inject_msg.has_inject_chrome_input());
  EXPECT_EQ(inject_msg.inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
  EXPECT_TRUE(inject_msg.inject_chrome_input().is_active());
  EXPECT_EQ(state_doc_id, doc_id);
  ASSERT_TRUE(state_msg.has_on_context_state_changed());
  EXPECT_EQ(
      state_msg.on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    ServiceWorkerHandler_RejectsInvalidAndSubframeDocumentIdsAndLazyConnects) {
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->UnregisterExtensionFrame(handler_);
  EXPECT_FALSE(user_data->HasBoundExtensionFrame());

  user_data->OnLensThumbnailCreated("data:image/png;base64,sw_lazy_crop");

  auto* bridge = ContextualTasksExtensionBridge::Get(GetProfile());
  ASSERT_NE(bridge, nullptr);

  mojo::Remote<mojom::ExtensionPageHandlerFactory> factory_remote;
  bridge->service_worker_handler().BindFactory(
      factory_remote.BindNewPipeAndPassReceiver());

  testing::NiceMock<MockContextualTasksExtensionPage> sw_page;
  mojo::Remote<mojom::ExtensionPageHandler> sw_handler;
  factory_remote->CreateExtensionPageHandler(
      sw_page.BindAndGetRemote(), sw_handler.BindNewPipeAndPassReceiver());

  // Create a child iframe to verify non-primary-main-frame document IDs are
  // rejected.
  ASSERT_TRUE(
      content::ExecJs(web_contents_,
                      "const iframe = document.createElement('iframe'); "
                      "document.body.appendChild(iframe);"));
  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(web_contents_->GetPrimaryMainFrame(), 0);
  ASSERT_NE(child_rfh, nullptr);
  std::string child_doc_id =
      extensions::ExtensionApiFrameIdMap::GetDocumentId(child_rfh).ToString();

  for (const std::optional<std::string>& bad_doc_id :
       {std::optional<std::string>(std::nullopt),
        std::optional<std::string>(""),
        std::optional<std::string>("not-a-valid-document-id"),
        std::optional<std::string>(child_doc_id)}) {
    base::test::TestFuture<mojo_base::ProtoWrapper> handshake_future;
    sw_handler->GetHandshakeMessage(bad_doc_id, handshake_future.GetCallback());
    mojo_base::ProtoWrapper handshake_wrapper = handshake_future.Take();
    auto ping_msg = handshake_wrapper.As<lens::ClientToAimMessage>();
    ASSERT_TRUE(ping_msg.has_value());
    EXPECT_FALSE(ping_msg->has_handshake_ping());
    EXPECT_FALSE(user_data->IsServiceWorkerPortConnected());
  }

  lens::SearchToClientMessage handshake_resp;
  handshake_resp.mutable_handshake_response()->set_auth_user_index(6);
  std::string resp_str;
  ASSERT_TRUE(handshake_resp.SerializeToString(&resp_str));
  std::vector<uint8_t> resp_bytes(resp_str.begin(), resp_str.end());

  // Invalid or subframe document IDs in OnWebviewMessage should be ignored.
  sw_handler->OnWebviewMessage(resp_bytes, std::nullopt);
  sw_handler->OnWebviewMessage(resp_bytes, "not-a-valid-document-id");
  sw_handler->OnWebviewMessage(resp_bytes, child_doc_id);
  sw_handler.FlushForTesting();
  EXPECT_FALSE(user_data->IsServiceWorkerPortConnected());
  EXPECT_FALSE(user_data->IsHandshakeCompleteForTesting());

  // Calling OnWebviewMessage with the primary main frame's document_id before
  // GetHandshakeMessage should lazily connect the document and flush pending
  // search messages.
  std::string main_doc_id = extensions::ExtensionApiFrameIdMap::GetDocumentId(
                                web_contents_->GetPrimaryMainFrame())
                                .ToString();
  EXPECT_CALL(*mock_controller_, SetAuthUserIndex(6));
  base::test::TestFuture<lens::ClientToSearchMessage,
                         std::optional<std::string>>
      inject_future;
  base::test::TestFuture<lens::ClientToSearchMessage,
                         std::optional<std::string>>
      state_future;
  EXPECT_CALL(sw_page, PostSearchMessageWithDocumentId(_, _))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper,
                    const std::optional<std::string>& received_doc_id) {
        auto parsed = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(parsed.has_value());
        inject_future.SetValue(*parsed, received_doc_id);
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper,
                    const std::optional<std::string>& received_doc_id) {
        auto parsed = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(parsed.has_value());
        state_future.SetValue(*parsed, received_doc_id);
      });

  sw_handler->OnWebviewMessage(resp_bytes, main_doc_id);
  auto [inject_msg, received_doc_id] = inject_future.Take();
  auto [state_msg, state_doc_id] = state_future.Take();

  EXPECT_TRUE(user_data->IsServiceWorkerPortConnected());
  EXPECT_TRUE(user_data->IsHandshakeCompleteForTesting());
  EXPECT_EQ(mock_session_handle_->auth_user_index(), 6u);
  EXPECT_EQ(received_doc_id, main_doc_id);
  ASSERT_TRUE(inject_msg.has_inject_chrome_input());
  EXPECT_EQ(inject_msg.inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
  EXPECT_TRUE(inject_msg.inject_chrome_input().is_active());
  EXPECT_EQ(state_doc_id, main_doc_id);
  ASSERT_TRUE(state_msg.has_on_context_state_changed());
  EXPECT_EQ(
      state_msg.on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    ServiceWorkerHandler_MountsLensCropWhenExtensionFrameBoundBeforeConnect) {
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);
  // Keep `handler_` bound (simulating `lens_button.html` loading before the
  // top-level AIM page connects its Service Worker port). Creating a Lens crop
  // before `OnDocumentConnected()` sets `is_lens_crop_mounted = true` via the
  // legacy extension frame fallback.
  ASSERT_TRUE(user_data->HasBoundExtensionFrame());
  user_data->OnLensThumbnailCreated("data:image/png;base64,prebound_crop");

  auto* bridge = ContextualTasksExtensionBridge::Get(GetProfile());
  ASSERT_NE(bridge, nullptr);

  mojo::Remote<mojom::ExtensionPageHandlerFactory> factory_remote;
  bridge->service_worker_handler().BindFactory(
      factory_remote.BindNewPipeAndPassReceiver());

  testing::NiceMock<MockContextualTasksExtensionPage> sw_page;
  mojo::Remote<mojom::ExtensionPageHandler> sw_handler;
  factory_remote->CreateExtensionPageHandler(
      sw_page.BindAndGetRemote(), sw_handler.BindNewPipeAndPassReceiver());

  std::string doc_id = extensions::ExtensionApiFrameIdMap::GetDocumentId(
                           web_contents_->GetPrimaryMainFrame())
                           .ToString();

  // Connecting the Service Worker port for the first time on the document must
  // reset `is_lens_crop_mounted` so `OnHandshakeComplete()` dispatches the
  // `InjectChromeInput(LENS_CHIP, true)` message over the Service Worker.
  base::test::TestFuture<mojo_base::ProtoWrapper> handshake_future;
  sw_handler->GetHandshakeMessage(doc_id, handshake_future.GetCallback());
  ASSERT_TRUE(handshake_future.Wait());
  EXPECT_TRUE(user_data->IsServiceWorkerPortConnected());

  base::test::TestFuture<lens::ClientToSearchMessage,
                         std::optional<std::string>>
      inject_future;
  base::test::TestFuture<lens::ClientToSearchMessage,
                         std::optional<std::string>>
      state_future;
  EXPECT_CALL(sw_page, PostSearchMessageWithDocumentId(_, _))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper,
                    const std::optional<std::string>& received_doc_id) {
        auto parsed = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(parsed.has_value());
        inject_future.SetValue(*parsed, received_doc_id);
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper,
                    const std::optional<std::string>& received_doc_id) {
        auto parsed = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(parsed.has_value());
        state_future.SetValue(*parsed, received_doc_id);
      });

  lens::SearchToClientMessage handshake_resp;
  handshake_resp.mutable_handshake_response()->set_auth_user_index(2);
  std::string resp_str;
  ASSERT_TRUE(handshake_resp.SerializeToString(&resp_str));
  std::vector<uint8_t> resp_bytes(resp_str.begin(), resp_str.end());

  sw_handler->OnWebviewMessage(resp_bytes, doc_id);
  auto [inject_msg, received_doc_id] = inject_future.Take();
  auto [state_msg, state_doc_id] = state_future.Take();

  EXPECT_TRUE(user_data->IsHandshakeCompleteForTesting());
  EXPECT_EQ(received_doc_id, doc_id);
  ASSERT_TRUE(inject_msg.has_inject_chrome_input());
  EXPECT_EQ(inject_msg.inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
  EXPECT_TRUE(inject_msg.inject_chrome_input().is_active());
  EXPECT_EQ(state_doc_id, doc_id);
  ASSERT_TRUE(state_msg.has_on_context_state_changed());
  EXPECT_EQ(
      state_msg.on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnPageChangedMessageResetsLensCropAndAllowsRemountingOnNewThread) {
  // Step 1: Emit first Lens crop on the current thread. Should emit mount
  // (LENS_CHIP, is_active=true) and CONTEXT_STATE_READY.
  base::RunLoop mount_run_loop1;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_inject_chrome_input());
        EXPECT_EQ(message->inject_chrome_input().input_type(),
                  lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
        EXPECT_TRUE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        mount_run_loop1.Quit();
      });
  EXPECT_CALL(mock_page_,
              OnLensCropUpdated(GURL("data:image/png;base64,crop1")))
      .Times(1);

  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,crop1");
  mount_run_loop1.Run();

  auto model = handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model);
  EXPECT_TRUE(model->lens_crop().has_value());

  // Attaching a delayed tab to the session notifies the user data, which mounts
  // the context library (CONTEXT_LIBRARY, is_active=true). The context state
  // stays CONTEXT_STATE_READY, so no OnContextStateChanged is emitted.
  base::RunLoop context_library_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_inject_chrome_input());
        EXPECT_EQ(
            message->inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
        EXPECT_TRUE(message->inject_chrome_input().is_active());
        context_library_run_loop.Quit();
      });
  mock_session_handle_->AddPendingDelayedTab(123, GURL("https://example.com"),
                                             "Example");
  context_library_run_loop.Run();

  contextual_search::TabInfo restored_tab;
  restored_tab.tab_id = 456;
  restored_tab.url = GURL("https://example.com/restored");
  restored_tab.title = "Restored";
  mock_session_handle_->SetRestoredTabs({restored_tab});
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  ASSERT_NE(user_data, nullptr);
  EXPECT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_EQ(mock_session_handle_->GetTabContextState().restored.size(), 1u);

  // Step 2: AIM sends OnPageChanged (e.g. on soft navigation to a new
  // thread or zero state). Chrome clears the Lens crop, closes Lens, clears
  // selected tabs, and emits unmount (LENS_CHIP, is_active=false), context
  // library unmount (CONTEXT_LIBRARY, is_active=false) + CONTEXT_STATE_NONE.
  EXPECT_CALL(*mock_lens_controller_, lens_overlay_controller())
      .WillRepeatedly(Return(mock_lens_overlay_controller_.get()));
  EXPECT_CALL(*mock_lens_overlay_controller_, ClearRegionSelection()).Times(1);
  EXPECT_CALL(
      *mock_lens_controller_,
      CloseLensAsync(
          lens::LensOverlayDismissalSource::kContextualTasksLensChipRemoved))
      .Times(1);

  base::RunLoop clear_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_inject_chrome_input());
        EXPECT_EQ(message->inject_chrome_input().input_type(),
                  lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
        EXPECT_FALSE(message->inject_chrome_input().is_active());
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
        clear_run_loop.Quit();
      });

  lens::SearchToClientMessage page_changed_msg;
  page_changed_msg.mutable_on_page_changed()->set_is_zero_state(true);
  const size_t size = page_changed_msg.ByteSizeLong();
  std::vector<uint8_t> serialized_page_changed(size);
  page_changed_msg.SerializeToArray(serialized_page_changed.data(), size);
  handler_->OnWebviewMessage(serialized_page_changed, std::nullopt);
  clear_run_loop.Run();

  EXPECT_FALSE(model->lens_crop().has_value());
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  EXPECT_TRUE(mock_session_handle_->GetTabContextState().restored.empty());

  // Step 3: Selecting a new Lens region on the new thread mounts LENS_CHIP
  // again (rather than assuming it is already mounted).
  base::RunLoop mount_run_loop2;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_inject_chrome_input());
        EXPECT_EQ(message->inject_chrome_input().input_type(),
                  lens::ClientToSearchMessage::InjectChromeInput::LENS_CHIP);
        EXPECT_TRUE(message->inject_chrome_input().is_active());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_READY);
        mount_run_loop2.Quit();
      });
  EXPECT_CALL(mock_page_,
              OnLensCropUpdated(GURL("data:image/png;base64,crop2")))
      .Times(1);

  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,crop2");
  mount_run_loop2.Run();
  EXPECT_TRUE(model->lens_crop().has_value());
}

class ContextualTasksExtensionHandlerTabBasedSuggestionsBrowserTest
    : public ContextualTasksExtensionHandlerBrowserTestBase {
 public:
  ContextualTasksExtensionHandlerTabBasedSuggestionsBrowserTest()
      : ContextualTasksExtensionHandlerBrowserTestBase(
            {kContextualTasks, kContextualTasksRearchitecture,
             kContextualTasksForceEntryPointEligibility,
             kContextualTasksTabBasedSuggestions},
            {}) {}
};

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerTabBasedSuggestionsBrowserTest,
    OnSuggestInputsChanged_ExplicitTabUploadDeleteAndSubmit) {
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  int32_t tab_id = active_tab->GetHandle().raw_value();

  ON_CALL(*mock_session_handle_, search_session_id())
      .WillByDefault(Return("test_suggest_session"));

  // 1. Adding an explicit tab starts uploading: emits CONTEXT_LIBRARY mount
  // and CONTEXT_STATE_UPLOADING, but does not emit OnSuggestInputsChanged while
  // the upload is still in flight.
  base::RunLoop add_run_loop;
  base::UnguessableToken token1;
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
            token1 = result.value();
          }));
  add_run_loop.Run();

  // 2. When the explicit tab upload completes with a valid FileInfo and
  // request_id, Chrome emits CONTEXT_STATE_READY followed by
  // OnSuggestInputsChanged containing the uploaded tab's AddedContext.
  contextual_search::FileInfo file_info1;
  file_info1.file_token = token1;
  lens::LensOverlayRequestId req_id1;
  req_id1.set_context_id(999);
  file_info1.request_id = req_id1;
  ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(
          Return(std::vector<contextual_search::FileInfo>{file_info1}));

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
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_suggest_inputs_changed());
        const auto& suggest_inputs = message->on_suggest_inputs_changed();
        ASSERT_EQ(suggest_inputs.added_contexts_size(), 1);
        EXPECT_EQ(suggest_inputs.added_contexts(0).search_session_id(),
                  "test_suggest_session");
        EXPECT_EQ(suggest_inputs.added_contexts(0).request_id().context_id(),
                  999);
        EXPECT_EQ(
            suggest_inputs.added_contexts(0).contextual_input_upload_type(),
            lens::LensOverlayContextualInputUploadType::
                CONTEXTUAL_INPUT_UPLOAD_TYPE_EXPLICIT);
        ready_run_loop.Quit();
      });

  user_data->OnContextUploadStatusChanged(
      token1, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  ready_run_loop.Run();

  // 3. Deleting the tab context emits CONTEXT_LIBRARY unmount,
  // CONTEXT_STATE_NONE, and OnSuggestInputsChanged with empty added_contexts.
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
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_suggest_inputs_changed());
        EXPECT_EQ(message->on_suggest_inputs_changed().added_contexts_size(),
                  0);
        delete_run_loop.Quit();
      });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      tab_id);
  delete_run_loop.Run();

  // 4. Re-add the tab and complete upload -> emits OnSuggestInputsChanged, then
  // submitting the query clears suggest inputs back to empty added_contexts.
  base::RunLoop readd_run_loop;
  base::UnguessableToken token2;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_inject_chrome_input());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_on_context_state_changed());
        readd_run_loop.Quit();
      });

  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      tab_id, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            ASSERT_TRUE(result.has_value());
            token2 = result.value();
          }));
  readd_run_loop.Run();

  contextual_search::FileInfo file_info2;
  file_info2.file_token = token2;
  lens::LensOverlayRequestId req_id2;
  req_id2.set_context_id(1000);
  file_info2.request_id = req_id2;
  ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(
          Return(std::vector<contextual_search::FileInfo>{file_info2}));

  base::RunLoop ready2_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        EXPECT_TRUE(message->has_on_context_state_changed());
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_suggest_inputs_changed());
        ASSERT_EQ(message->on_suggest_inputs_changed().added_contexts_size(),
                  1);
        EXPECT_EQ(message->on_suggest_inputs_changed()
                      .added_contexts(0)
                      .request_id()
                      .context_id(),
                  1000);
        ready2_run_loop.Quit();
      });

  user_data->OnContextUploadStatusChanged(
      token2, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  ready2_run_loop.Run();

  // The submitted tab is still shown by the context library, so it stays
  // mounted.
  base::RunLoop submit_run_loop;
  EXPECT_CALL(mock_page_, PostSearchMessage(_))
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_submit_query_response());
        ASSERT_EQ(message->on_submit_query_response().added_contexts_size(), 1);
        EXPECT_EQ(message->on_submit_query_response()
                      .added_contexts(0)
                      .request_id()
                      .context_id(),
                  1000);
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_context_state_changed());
        EXPECT_EQ(message->on_context_state_changed().context_state(),
                  lens::ClientToSearchMessage::OnContextStateChanged::
                      CONTEXT_STATE_NONE);
      })
      .WillOnce([&](mojo_base::ProtoWrapper wrapper) {
        auto message = wrapper.As<lens::ClientToSearchMessage>();
        ASSERT_TRUE(message.has_value());
        ASSERT_TRUE(message->has_on_suggest_inputs_changed());
        EXPECT_EQ(message->on_suggest_inputs_changed().added_contexts_size(),
                  0);
        submit_run_loop.Quit();
      });

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  SimulateUserInteraction();
  handler_->OnWebviewMessage(serialized_message, std::nullopt);
  submit_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerTabBasedSuggestionsBrowserTest,
    ZeroIframes_OnSuggestInputsChangedQueuedUntilHandshake) {
  auto* user_data =
      ContextualTasksWebContentsUserData::GetOrCreateForWebContents(
          web_contents_);
  ASSERT_NE(user_data, nullptr);

  // Unregister the primary extension frame handler to simulate 0 extension
  // iframes mounted in the page (Service Worker Port mode).
  user_data->UnregisterExtensionFrame(handler_);
  EXPECT_FALSE(user_data->HasBoundExtensionFrame());
  user_data->OnDocumentConnected(web_contents_->GetPrimaryMainFrame(),
                                 base::DoNothing());

  std::vector<lens::ClientToSearchMessage> dispatched_messages;
  user_data->SetSearchMessageDispatcherForTesting(base::BindLambdaForTesting(
      [&dispatched_messages](const lens::ClientToSearchMessage& message) {
        dispatched_messages.push_back(message);
      }));

  // Attach an explicit tab and finish upload before handshake completes.
  contextual_search::FileInfo file_info;
  file_info.file_token = base::UnguessableToken::Create();
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(888);
  file_info.request_id = req_id;

  ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(
          Return(std::vector<contextual_search::FileInfo>{file_info}));
  ON_CALL(*mock_session_handle_, search_session_id())
      .WillByDefault(Return("zero_iframe_suggest_session"));

  // Attaching the tab notifies `user_data` of the tab context change, which
  // updates its context library input state.
  user_data->OnTabContextUploadStarted(file_info.file_token);
  AttachActiveTab(file_info.file_token);
  user_data->OnContextUploadStatusChanged(
      file_info.file_token, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  EXPECT_TRUE(dispatched_messages.empty());

  lens::SearchToClientMessage handshake_resp;
  handshake_resp.mutable_handshake_response()->set_auth_user_index(0);
  std::string resp_str;
  ASSERT_TRUE(handshake_resp.SerializeToString(&resp_str));
  std::vector<uint8_t> resp_bytes(resp_str.begin(), resp_str.end());

  EXPECT_TRUE(user_data->OnSearchMessageReceived(resp_bytes));
  ASSERT_EQ(dispatched_messages.size(), 4u);
  EXPECT_TRUE(dispatched_messages[0].has_inject_chrome_input());
  EXPECT_EQ(dispatched_messages[0].inject_chrome_input().input_type(),
            lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
  EXPECT_TRUE(dispatched_messages[0].inject_chrome_input().is_active());
  ASSERT_TRUE(dispatched_messages[1].has_on_context_state_changed());
  EXPECT_EQ(dispatched_messages[1].on_context_state_changed().context_state(),
            lens::ClientToSearchMessage::OnContextStateChanged::
                CONTEXT_STATE_UPLOADING);
  ASSERT_TRUE(dispatched_messages[2].has_on_context_state_changed());
  EXPECT_EQ(
      dispatched_messages[2].on_context_state_changed().context_state(),
      lens::ClientToSearchMessage::OnContextStateChanged::CONTEXT_STATE_READY);
  ASSERT_TRUE(dispatched_messages[3].has_on_suggest_inputs_changed());
  const auto& suggest_inputs =
      dispatched_messages[3].on_suggest_inputs_changed();
  ASSERT_EQ(suggest_inputs.added_contexts_size(), 1);
  EXPECT_EQ(suggest_inputs.added_contexts(0).search_session_id(),
            "zero_iframe_suggest_session");
  EXPECT_EQ(suggest_inputs.added_contexts(0).request_id().context_id(), 888);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    GetRecentTabs_IncludesAttachedInternalTabAndAllowsDeselection) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL(chrome::kChromeUINewTabURL),
      WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* ntp_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(ntp_tab, nullptr);
  const int32_t ntp_tab_handle_id = ntp_tab->GetHandle().raw_value();
  const SessionID ntp_session_id =
      sessions::SessionTabHelper::IdForTab(ntp_tab->GetContents());
  ASSERT_TRUE(ntp_session_id.is_valid());

  // 1. Before the internal tab is attached to the session, `GetRecentTabs()`
  // excludes it.
  {
    base::RunLoop run_loop;
    static_cast<searchbox::mojom::PageHandler*>(handler_)->GetRecentTabs(
        base::BindLambdaForTesting(
            [&run_loop, ntp_tab_handle_id](
                std::vector<searchbox::mojom::TabInfoPtr> tabs) {
              EXPECT_TRUE(std::ranges::none_of(
                  tabs,
                  [ntp_tab_handle_id](const searchbox::mojom::TabInfoPtr& t) {
                    return t->tab_id == ntp_tab_handle_id;
                  }));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // 2. Simulate Chromnient (Lens Overlay) attaching and submitting the NTP tab
  // directly on the session handle (without going through `AddTabContext()`),
  // followed by AIM echoing the tab back via `UpdateThreadContextLibrary`
  // (`SetRestoredTabs()`). The tab now exists in both `state.attached` and
  // `state.restored`.
  const base::UnguessableToken ntp_token = base::UnguessableToken::Create();
  lens::LensOverlayRequestId ntp_req_id;
  ntp_req_id.set_uuid(8888);
  mock_session_handle_->SetSubmittedContextTokens({ntp_token});
  {
    base::RunLoop notify_loop;
    EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
        .WillOnce([&notify_loop, ntp_tab_handle_id](
                      std::vector<searchbox::mojom::TabInfoPtr> tabs,
                      const std::vector<int32_t>& submitted_tab_ids) {
          ASSERT_EQ(tabs.size(), 1u);
          EXPECT_EQ(tabs[0]->tab_id, ntp_tab_handle_id);
          EXPECT_THAT(submitted_tab_ids,
                      testing::ElementsAre(ntp_tab_handle_id));
          notify_loop.Quit();
        });
    mock_session_handle_->SetPersistedTabs(
        {{ntp_session_id, {ntp_token, ntp_req_id}}});
    notify_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

  contextual_search::TabInfo restored_ntp;
  restored_ntp.tab_id = ntp_session_id.id();
  restored_ntp.url = GURL(chrome::kChromeUINewTabURL);
  restored_ntp.title = "New Tab";
  {
    base::RunLoop restore_notify_loop;
    EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
        .WillOnce([&restore_notify_loop, ntp_tab_handle_id](
                      std::vector<searchbox::mojom::TabInfoPtr> tabs,
                      const std::vector<int32_t>& submitted_tab_ids) {
          ASSERT_EQ(tabs.size(), 1u);
          EXPECT_EQ(tabs[0]->tab_id, ntp_tab_handle_id);
          EXPECT_THAT(submitted_tab_ids,
                      testing::ElementsAre(ntp_tab_handle_id));
          restore_notify_loop.Quit();
        });
    mock_session_handle_->SetRestoredTabs({restored_ntp});
    restore_notify_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }
  ASSERT_EQ(mock_session_handle_->GetTabContextState().attached.size(), 1u);
  ASSERT_EQ(mock_session_handle_->GetTabContextState().restored.size(), 1u);

  // 3. `GetRecentTabs()` now includes the attached NTP tab so the user can see
  // and unselect it in the tab picker.
  {
    base::RunLoop run_loop;
    static_cast<searchbox::mojom::PageHandler*>(handler_)->GetRecentTabs(
        base::BindLambdaForTesting(
            [&run_loop, ntp_tab_handle_id](
                std::vector<searchbox::mojom::TabInfoPtr> tabs) {
              EXPECT_TRUE(std::ranges::any_of(
                  tabs,
                  [ntp_tab_handle_id](const searchbox::mojom::TabInfoPtr& t) {
                    return t->tab_id == ntp_tab_handle_id;
                  }));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // 4. Unselecting the submitted NTP tab once via `DeleteTabContext()` removes
  // it from both `attached` and `restored` immediately on the first click and
  // notifies `OnTabContextUpdated()` with an empty tab list (instead of
  // re-adding the tab from `state.restored`).
  {
    base::RunLoop deselect_notify_loop;
    EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
        .WillOnce([&deselect_notify_loop](
                      std::vector<searchbox::mojom::TabInfoPtr> tabs,
                      const std::vector<int32_t>& submitted_tab_ids) {
          EXPECT_TRUE(tabs.empty());
          EXPECT_TRUE(submitted_tab_ids.empty());
          deselect_notify_loop.Quit();
        });
    static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
        ntp_tab_handle_id);
    deselect_notify_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }
  EXPECT_TRUE(mock_session_handle_->GetTabContextState().attached.empty());
  EXPECT_TRUE(mock_session_handle_->GetTabContextState().restored.empty());
  EXPECT_TRUE(mock_session_handle_->GetSubmittedContextTokens().empty());

  // 5. Once unselected, `GetRecentTabs()` excludes the internal NTP tab again.
  {
    base::RunLoop run_loop;
    static_cast<searchbox::mojom::PageHandler*>(handler_)->GetRecentTabs(
        base::BindLambdaForTesting(
            [&run_loop, ntp_tab_handle_id](
                std::vector<searchbox::mojom::TabInfoPtr> tabs) {
              EXPECT_TRUE(std::ranges::none_of(
                  tabs,
                  [ntp_tab_handle_id](const searchbox::mojom::TabInfoPtr& t) {
                    return t->tab_id == ntp_tab_handle_id;
                  }));
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // 6. Submitting a follow-up query reports the unselected tab's request ID in
  // `removed_contexts` to AIM even without `kContextManagementInComposebox`.
  {
    base::RunLoop submit_loop;
    EXPECT_CALL(mock_page_, PostSearchMessage(_))
        .WillRepeatedly([&submit_loop](mojo_base::ProtoWrapper wrapper) {
          auto message = wrapper.As<lens::ClientToSearchMessage>();
          if (!message.has_value() ||
              !message->has_on_submit_query_response()) {
            return;
          }
          const auto& response = message->on_submit_query_response();
          ASSERT_EQ(response.removed_contexts_size(), 1);
          EXPECT_EQ(response.removed_contexts(0).request_id().uuid(), 8888u);
          submit_loop.Quit();
        });
    lens::SearchToClientMessage request;
    request.mutable_on_submit_query_request();
    const size_t size = request.ByteSizeLong();
    std::vector<uint8_t> serialized_message(size);
    request.SerializeToArray(serialized_message.data(), size);
    SimulateUserInteraction();
    handler_->OnWebviewMessage(serialized_message, std::nullopt);
    submit_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    DeleteTabContext_RemovesRestoredTabAndAllowsReselection) {
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), GURL("https://www.google.com/restored_page"),
      static_cast<WindowOpenDisposition>(3),
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* tab = TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(tab, nullptr);
  const int32_t tab_handle_id = tab->GetHandle().raw_value();
  const SessionID session_id =
      sessions::SessionTabHelper::IdForTab(tab->GetContents());
  ASSERT_TRUE(session_id.is_valid());

  // 1. Seed a restored tab backed by a submitted `FileInfo` (e.g. from initial
  // CSB query or thread history where `attached` is empty).
  const base::UnguessableToken submitted_token =
      base::UnguessableToken::Create();
  lens::LensOverlayRequestId req_id;
  req_id.set_uuid(7777);
  contextual_search::FileInfo submitted_file_info;
  submitted_file_info.file_token = submitted_token;
  submitted_file_info.tab_session_id = session_id;
  submitted_file_info.tab_url = GURL("https://www.google.com/restored_page");
  submitted_file_info.tab_title = "Restored Page";
  submitted_file_info.request_id = req_id;
  ON_CALL(*mock_session_handle_, GetSubmittedContextFileInfos())
      .WillByDefault(Return(
          std::vector<contextual_search::FileInfo>{submitted_file_info}));
  mock_session_handle_->SetSubmittedContextTokens({submitted_token});

  contextual_search::TabInfo restored_tab;
  restored_tab.tab_id = session_id.id();
  restored_tab.url = GURL("https://www.google.com/restored_page");
  restored_tab.title = "Restored Page";
  {
    base::RunLoop restore_loop;
    EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
        .WillOnce([&restore_loop, tab_handle_id](
                      std::vector<searchbox::mojom::TabInfoPtr> tabs,
                      const std::vector<int32_t>& submitted_tab_ids) {
          ASSERT_EQ(tabs.size(), 1u);
          EXPECT_EQ(tabs[0]->tab_id, tab_handle_id);
          restore_loop.Quit();
        });
    mock_session_handle_->SetRestoredTabs({restored_tab});
    restore_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

  // 2. Unselecting the restored tab removes it immediately on first click.
  {
    base::RunLoop deselect_loop;
    EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
        .WillOnce(
            [&deselect_loop](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                             const std::vector<int32_t>& submitted_tab_ids) {
              EXPECT_TRUE(tabs.empty());
              EXPECT_TRUE(submitted_tab_ids.empty());
              deselect_loop.Quit();
            });
    static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
        tab_handle_id);
    deselect_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

  // 3. Re-selecting the same tab via `AddTabContext()` clears its deselection
  // and re-attaches it on the first click, and `TakeRemovedContexts()` does not
  // report the re-selected tab as removed.
  {
    base::RunLoop reselect_notify_loop;
    EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
        .WillOnce([&reselect_notify_loop, tab_handle_id](
                      std::vector<searchbox::mojom::TabInfoPtr> tabs,
                      const std::vector<int32_t>& submitted_tab_ids) {
          ASSERT_EQ(tabs.size(), 1u);
          EXPECT_EQ(tabs[0]->tab_id, tab_handle_id);
          reselect_notify_loop.Quit();
        });
    base::RunLoop add_loop;
    static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
        tab_handle_id, /*delay_upload=*/false,
        searchbox::mojom::TabAttachmentSource::kContextMenu,
        base::BindLambdaForTesting(
            [&add_loop](
                base::expected<base::UnguessableToken,
                               contextual_search::ContextUploadErrorType>
                    result) {
              EXPECT_TRUE(result.has_value());
              add_loop.Quit();
            }));
    add_loop.Run();
    reselect_notify_loop.Run();
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }
  EXPECT_TRUE(mock_session_handle_->TakeRemovedContexts().empty());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnPageChanged_ZeroStateInSidePanel_AddsImplicitTabAndUpdatesOnNavigation) {
  GURL url1;
  GURL url2;
  auto* user_data = StartTestServerAndOpenSidePanel(&url1, &url2);
  ASSERT_NE(user_data, nullptr);
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  EXPECT_FALSE(user_data->GetAutoSuggestedTabHandle().has_value());

  // 1. `OnPageChanged(false)` does not add implicit context.
  EXPECT_CALL(mock_panel_page_, PostSearchMessage(_)).Times(0);
  SendOnPageChanged(false);
  mock_panel_page_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_panel_page_);
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  EXPECT_FALSE(user_data->GetAutoSuggestedTabHandle().has_value());

  auto expect_context_library = [this](bool active, base::RunLoop& loop) {
    EXPECT_CALL(mock_panel_page_, PostSearchMessage(_))
        .WillOnce([active](mojo_base::ProtoWrapper wrapper) {
          auto message = wrapper.As<lens::ClientToSearchMessage>();
          ASSERT_TRUE(message.has_value());
          EXPECT_EQ(
              message->inject_chrome_input().input_type(),
              lens::ClientToSearchMessage::InjectChromeInput::CONTEXT_LIBRARY);
          EXPECT_EQ(message->inject_chrome_input().is_active(), active);
        })
        .WillOnce([active, &loop](mojo_base::ProtoWrapper wrapper) {
          auto message = wrapper.As<lens::ClientToSearchMessage>();
          ASSERT_TRUE(message.has_value());
          EXPECT_EQ(message->on_context_state_changed().context_state(),
                    active ? lens::ClientToSearchMessage::
                                 OnContextStateChanged::CONTEXT_STATE_READY
                           : lens::ClientToSearchMessage::
                                 OnContextStateChanged::CONTEXT_STATE_NONE);
          loop.Quit();
        });
  };

  // 2. `OnPageChanged(true)` attaches the active tab as pending delayed.
  base::RunLoop zero_state_loop;
  EXPECT_CALL(mock_panel_page_, OnTabContextUpdated(_, _))
      .WillOnce([&url1](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                        const std::vector<int32_t>& submitted_tab_ids) {
        ASSERT_EQ(tabs.size(), 1u);
        EXPECT_EQ(tabs[0]->url, url1);
        EXPECT_TRUE(submitted_tab_ids.empty());
      });
  expect_context_library(true, zero_state_loop);
  SendOnPageChanged(true);
  zero_state_loop.Run();
  mock_panel_page_.FlushForTesting();
  testing::Mock::VerifyAndClearExpectations(&mock_panel_page_);

  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_TRUE(user_data->GetSelectedTabs()[0].pending_delayed_upload);
  EXPECT_EQ(user_data->GetSelectedTabs()[0].url, url1);
  EXPECT_EQ(user_data->GetAutoSuggestedTabHandle(),
            TabListInterface::From(browser())->GetActiveTab()->GetHandle());

  // 3. Navigating the active tab while in zero state updates the implicit tab.
  base::RunLoop nav_loop;
  EXPECT_CALL(mock_panel_page_, OnTabContextUpdated(_, _))
      .WillRepeatedly(
          [&url2, &nav_loop](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                             const std::vector<int32_t>& submitted_tab_ids) {
            if (tabs.size() == 1u && tabs[0]->url == url2 &&
                tabs[0]->title == "Title Of Awesomeness") {
              EXPECT_TRUE(submitted_tab_ids.empty());
              nav_loop.Quit();
            }
          });
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  nav_loop.Run();
  testing::Mock::VerifyAndClearExpectations(&mock_panel_page_);

  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_TRUE(user_data->GetSelectedTabs()[0].pending_delayed_upload);
  EXPECT_EQ(user_data->GetSelectedTabs()[0].url, url2);
  EXPECT_EQ(user_data->GetSelectedTabs()[0].title, "Title Of Awesomeness");

  // 4. Navigating to `about:blank` removes the implicit tab and unmounts UI.
  base::RunLoop unmount_loop;
  expect_context_library(false, unmount_loop);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  unmount_loop.Run();
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  EXPECT_FALSE(user_data->GetAutoSuggestedTabHandle().has_value());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnPageChanged_ZeroStateNotInSidePanel_DoesNotAddImplicitTab) {
  ASSERT_TRUE(embedded_test_server()->Start());
  handler_ = nullptr;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));

  content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();
  ContextualTasksExtensionHandler::CreateForCurrentDocument(rfh);
  handler_ = ContextualTasksExtensionHandler::GetForCurrentDocument(rfh);
  ASSERT_NE(handler_, nullptr);
  testing::NiceMock<MockContextualTasksExtensionPage> page;
  mojo::PendingReceiver<mojom::ExtensionPageHandler> page_handler_receiver;
  handler_->CreateExtensionPageHandler(page.BindAndGetRemote(),
                                       std::move(page_handler_receiver));

  EXPECT_CALL(page, PostSearchMessage(_)).Times(0);
  SendOnPageChanged(true);
  page.FlushForTesting();

  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  ASSERT_NE(user_data, nullptr);
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnPageChanged_DismissedImplicitTabBlocklistedUntilNextOnPageChanged) {
  GURL url1;
  GURL url2;
  auto* panel_controller = ContextualTasksPanelController::From(browser());
  auto* user_data = StartTestServerAndOpenSidePanel(&url1, &url2);
  ASSERT_NE(user_data, nullptr);

  SendOnPageChanged(true);
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  // 1. Dismissing via `DeleteTabContext()` blocklists `url1` on this thread.
  const tabs::TabHandle active_tab_handle =
      TabListInterface::From(browser())->GetActiveTab()->GetHandle();
  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(
      active_tab_handle.raw_value());
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  user_data->UpdateImplicitTabContext();
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  // 2. Navigating to `url2` adds `url2`; dismissing via `DeleteContext()`
  // blocklists `url2`, and manually sharing `url2` unblocks it.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteContext(
      user_data->GetSelectedTabs()[0].context_token,
      /*from_automatic_chip=*/true);
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  user_data->UpdateImplicitTabContext();
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  base::RunLoop share_tab_loop;
  EXPECT_CALL(*mock_session_handle_, StartTabContextUploadFlow(_, _, _))
      .WillOnce([&share_tab_loop](const base::UnguessableToken&,
                                  std::unique_ptr<lens::ContextualInputData>,
                                  std::optional<lens::ImageEncodingOptions>) {
        share_tab_loop.Quit();
      });
  AllowSessionTokenCreation();
  panel_controller->ShareTabsWithActiveOrNewThread({active_tab_handle});
  share_tab_loop.Run();
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_EQ(user_data->GetSelectedTabs()[0].url, url2);
  // 3. `ClearFiles(false)` keeps `url1` blocklisted, while manual
  // `AddTabContext(..., delay_upload=false, ...)` unblocks and attaches `url1`.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url1));
  static_cast<searchbox::mojom::PageHandler*>(handler_)->ClearFiles(
      /*should_block_auto_suggested_tabs=*/false);
  user_data->UpdateImplicitTabContext();
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  base::test::TestFuture<base::expected<
      base::UnguessableToken, contextual_search::ContextUploadErrorType>>
      manual_add_future;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      active_tab_handle.raw_value(), /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      manual_add_future.GetCallback());
  ASSERT_TRUE(manual_add_future.Take().has_value());
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_FALSE(user_data->GetAutoSuggestedTabHandle().has_value());
  // 4. `ClearFiles(true)` blocklists `url1` until next `OnPageChanged(true)`,
  // and `SetTaskId()` cleans up when switching tasks.
  SendOnPageChanged(true);
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  static_cast<searchbox::mojom::PageHandler*>(handler_)->ClearFiles(
      /*should_block_auto_suggested_tabs=*/true);
  user_data->UpdateImplicitTabContext();
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  SendOnPageChanged(true);
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  user_data->SetTaskId(*side_panel_task_id_);
  EXPECT_EQ(user_data->GetSelectedTabs().size(), 1u);
  user_data->SetTaskId(base::Uuid::GenerateRandomV4());
  EXPECT_TRUE(user_data->GetSelectedTabs().empty());
  EXPECT_FALSE(user_data->GetAutoSuggestedTabHandle().has_value());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnPageChanged_PreservesPreStagedShareTabOnInitialZeroStateLoad) {
  ASSERT_TRUE(embedded_test_server()->Start());
  handler_ = nullptr;
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/title1.html")));
  auto* panel_controller = ContextualTasksPanelController::From(browser());
  const tabs::TabHandle tab1_handle =
      TabListInterface::From(browser())->GetActiveTab()->GetHandle();
  panel_controller->ShareTabsWithActiveOrNewThread({tab1_handle});
  content::WebContents* panel_contents =
      panel_controller->GetActiveWebContents();
  ASSERT_NE(panel_contents, nullptr);
  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(panel_contents);
  ASSERT_NE(user_data, nullptr);
  ASSERT_TRUE(base::test::RunUntil([&] {
    return user_data->GetSelectedTabs().size() == 1u &&
           browser()->tab_strip_model()->count() == 2;
  }));
  browser()->tab_strip_model()->ActivateTabAt(0);
  ShowSidePanel();
  EXPECT_FALSE(user_data->GetSelectedTabs()[0].pending_delayed_upload);

  lens::SearchToClientMessage msg;
  msg.mutable_on_page_changed()->set_is_zero_state(true);
  std::vector<uint8_t> bytes(msg.ByteSizeLong());
  msg.SerializeToArray(bytes.data(), bytes.size());
  EXPECT_TRUE(user_data->OnSearchMessageReceived(bytes));
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_FALSE(user_data->GetSelectedTabs()[0].pending_delayed_upload);
  EXPECT_TRUE(user_data->OnSearchMessageReceived(bytes));
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_TRUE(user_data->GetSelectedTabs()[0].pending_delayed_upload);
  EXPECT_EQ(panel_controller->GetAutoSuggestedTabHandle(), tab1_handle);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnPageChanged_ZeroStateTabSwitchesManualTabsCloseAndDetach) {
  GURL url1;
  GURL url2;
  auto* panel_controller = ContextualTasksPanelController::From(browser());
  auto* user_data = StartTestServerAndOpenSidePanel(&url1, &url2);
  ASSERT_NE(user_data, nullptr);

  SendOnPageChanged(true);
  const tabs::TabHandle tab1_handle =
      TabListInterface::From(browser())->GetActiveTab()->GetHandle();
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_EQ(user_data->GetSelectedTabs()[0].url, url1);
  EXPECT_EQ(panel_controller->GetAutoSuggestedTabHandle(), tab1_handle);

  // Activate a second associated tab: implicit tab updates to `tab2` (`url2`).
  ASSERT_TRUE(AddTabAtIndex(1, url2, ui::PAGE_TRANSITION_TYPED));
  ContextualTasksServiceFactory::GetForProfile(browser()->GetProfile())
      ->AssociateTabWithTask(
          *side_panel_task_id_,
          sessions::SessionTabHelper::IdForTab(
              browser()->tab_strip_model()->GetActiveWebContents()));
  ShowSidePanel();
  const tabs::TabHandle tab2_handle =
      TabListInterface::From(browser())->GetActiveTab()->GetHandle();
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_EQ(user_data->GetSelectedTabs()[0].url, url2);
  EXPECT_EQ(panel_controller->GetAutoSuggestedTabHandle(), tab2_handle);

  // Manually sharing `tab1` keeps `tab2` auto-suggested; activating `tab1`
  // drops `tab2`'s auto-suggestion while preserving manual `tab1`.
  base::RunLoop tab1_upload_loop;
  EXPECT_CALL(*mock_session_handle_, StartTabContextUploadFlow(_, _, _))
      .WillOnce([&tab1_upload_loop](const base::UnguessableToken&,
                                    std::unique_ptr<lens::ContextualInputData>,
                                    std::optional<lens::ImageEncodingOptions>) {
        tab1_upload_loop.Quit();
      });
  AllowSessionTokenCreation();
  panel_controller->ShareTabsWithActiveOrNewThread({tab1_handle});
  tab1_upload_loop.Run();
  EXPECT_EQ(user_data->GetSelectedTabs().size(), 2u);

  browser()->tab_strip_model()->ActivateTabAt(0);
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_EQ(user_data->GetSelectedTabs()[0].url, url1);
  EXPECT_FALSE(user_data->GetSelectedTabs()[0].pending_delayed_upload);
  EXPECT_FALSE(panel_controller->GetAutoSuggestedTabHandle().has_value());

  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_EQ(user_data->GetSelectedTabs().size(), 2u);
  EXPECT_EQ(panel_controller->GetAutoSuggestedTabHandle(), tab2_handle);

  // Unassociated tab hides panel and removes implicit tab; returning restores.
  ASSERT_TRUE(AddTabAtIndex(2, embedded_test_server()->GetURL("/empty.html"),
                            ui::PAGE_TRANSITION_TYPED));
  EXPECT_FALSE(panel_controller->IsPanelOpenForContextualTask());
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_EQ(user_data->GetSelectedTabs()[0].url, url1);
  EXPECT_FALSE(panel_controller->GetAutoSuggestedTabHandle().has_value());

  browser()->tab_strip_model()->ActivateTabAt(1);
  EXPECT_TRUE(panel_controller->IsPanelOpenForContextualTask());
  EXPECT_EQ(user_data->GetSelectedTabs().size(), 2u);
  EXPECT_EQ(panel_controller->GetAutoSuggestedTabHandle(), tab2_handle);

  // Detach and re-transfer remove and restore implicit context cleanly.
  const base::Uuid task_id = *side_panel_task_id_;
  side_panel_task_id_.reset();
  std::unique_ptr<content::WebContents> detached_contents =
      panel_controller->DetachWebContentsForTask(task_id);
  ASSERT_NE(detached_contents, nullptr);
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 1u);
  EXPECT_EQ(user_data->GetSelectedTabs()[0].url, url1);
  EXPECT_FALSE(panel_controller->GetAutoSuggestedTabHandle().has_value());

  panel_controller->TransferWebContentsFromTab(task_id,
                                               std::move(detached_contents));
  ASSERT_EQ(user_data->GetSelectedTabs().size(), 2u);
  EXPECT_EQ(panel_controller->GetAutoSuggestedTabHandle(), tab2_handle);
  base::UnguessableToken manual_tab1_token;
  for (const contextual_search::TabInfo& tab : user_data->GetSelectedTabs()) {
    if (tab.url == url1) {
      manual_tab1_token = tab.context_token;
    }
  }
  ASSERT_FALSE(manual_tab1_token.is_empty());
  mock_panel_page_.FlushForTesting();
  base::WeakPtr<contextual_search::InputStateModel> model =
      handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_NE(model, nullptr);
  int close_context_updates = 0;
  base::CallbackListSubscription close_sub =
      model->Subscribe(base::BindLambdaForTesting(
          [&close_context_updates](const omnibox::InputState&) {
            ++close_context_updates;
          }));
  std::optional<contextual_search::TabContextState> close_tab_state;
  base::CallbackListSubscription tab_state_sub =
      mock_session_handle_->SubscribeTabContext(base::BindLambdaForTesting(
          [&close_tab_state](const contextual_search::TabContextState& state) {
            close_tab_state = state;
          }));
  EXPECT_CALL(*mock_controller_, DeleteFile(manual_tab1_token)).Times(0);

  handler_ = nullptr;
  panel_web_contents_ = nullptr;
  mock_session_handle_ = nullptr;
  panel_controller->Close();
  EXPECT_EQ(close_context_updates, 1);
  ASSERT_TRUE(close_tab_state.has_value());
  ASSERT_EQ(close_tab_state->attached.size(), 1u);
  EXPECT_EQ(close_tab_state->attached[0].context_token, manual_tab1_token);
  EXPECT_FALSE(panel_controller->GetAutoSuggestedTabHandle().has_value());
  testing::Mock::VerifyAndClearExpectations(mock_controller_.get());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       BindTrackedElementHandler_TracksFaviconsElement) {
  EmbedHandlerInSidePanel();
  mojo::Remote<tracked_element::mojom::TrackedElementHandler> remote;
  handler_->BindTrackedElementHandler(remote.BindNewPipeAndPassReceiver());

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  const ui::ElementContext context =
      BrowserElements::From(browser())->GetContext();

  EXPECT_FALSE(tracker->GetFirstMatchingElement(
      kContextualTasksFaviconsElementId, context));

  remote->TrackedElementVisibilityChanged(
      tracked_element::mojom::TrackedElementIdentifier::New(
          kContextualTasksFaviconsElementId.GetName(), "0"),
      true, gfx::RectF(10, 20, 100, 30));
  remote.FlushForTesting();

  auto* const element = tracker->GetFirstMatchingElement(
      kContextualTasksFaviconsElementId, context);
  ASSERT_NE(element, nullptr);
  EXPECT_EQ(element->identifier(), kContextualTasksFaviconsElementId);

  remote->TrackedElementVisibilityChanged(
      tracked_element::mojom::TrackedElementIdentifier::New(
          kContextualTasksFaviconsElementId.GetName(), "0"),
      false, gfx::RectF());
  remote.FlushForTesting();

  EXPECT_FALSE(tracker->GetFirstMatchingElement(
      kContextualTasksFaviconsElementId, context));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       BindTrackedElementHandler_NotInSidePanel_DoesNothing) {
  mojo::Remote<tracked_element::mojom::TrackedElementHandler> remote;
  handler_->BindTrackedElementHandler(remote.BindNewPipeAndPassReceiver());

  auto* const tracker = ui::ElementTracker::GetElementTracker();
  const ui::ElementContext context =
      BrowserElements::From(browser())->GetContext();

  remote->TrackedElementVisibilityChanged(
      tracked_element::mojom::TrackedElementIdentifier::New(
          kContextualTasksFaviconsElementId.GetName(), "0"),
      true, gfx::RectF(10, 20, 100, 30));
  remote.FlushForTesting();

  EXPECT_EQ(tracker->GetFirstMatchingElement(kContextualTasksFaviconsElementId,
                                             context),
            nullptr);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       PersistedTabs_ShownInContextLibrary) {
  const base::UnguessableToken token = base::UnguessableToken::Create();
  SessionID active_tab_id = sessions::SessionTabHelper::IdForTab(web_contents_);
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(active_tab, nullptr);
  const int32_t active_tab_handle_id = active_tab->GetHandle().raw_value();

  contextual_search::FileInfo persisted_file_info;
  persisted_file_info.file_token = token;
  persisted_file_info.tab_session_id = active_tab_id;
  persisted_file_info.tab_url = GURL("https://example.com/persisted");
  persisted_file_info.tab_title = "Persisted Tab";
  lens::LensOverlayRequestId req_id;
  req_id.set_context_id(555);
  persisted_file_info.request_id = req_id;
  ON_CALL(*mock_controller_, GetFileInfo(token))
      .WillByDefault(Return(&persisted_file_info));

  // Persisted tabs are keyed by session ID, but are sent to the frontend by
  // tab handle ID, which it uses to identify tabs.
  base::RunLoop tab_context_run_loop;
  EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
      .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                    const std::vector<int32_t>& submitted_tab_ids) {
        ASSERT_EQ(tabs.size(), 1u);
        EXPECT_EQ(tabs[0]->tab_id, active_tab_handle_id);
        // The URL and title come from the tab's FileInfo.
        EXPECT_EQ(tabs[0]->url, GURL("https://example.com/persisted"));
        EXPECT_EQ(tabs[0]->title, "Persisted Tab");
        EXPECT_THAT(submitted_tab_ids,
                    testing::ElementsAre(active_tab_handle_id));
        tab_context_run_loop.Quit();
      });

  mock_session_handle_->SetPersistedTabs({{active_tab_id, {token, req_id}}});
  tab_context_run_loop.Run();

  // Completing handshake when `uploaded_context_tokens_` is empty (e.g. on SRP
  // after CreateSearchUrl / SetPersistedTabs) still mounts CONTEXT_LIBRARY.
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
        handshake_run_loop.Quit();
      });

  lens::SearchToClientMessage handshake_msg;
  handshake_msg.mutable_handshake_response()->set_auth_user_index(0);
  const size_t size = handshake_msg.ByteSizeLong();
  std::vector<uint8_t> serialized_handshake(size);
  handshake_msg.SerializeToArray(serialized_handshake.data(), size);
  handler_->OnWebviewMessage(serialized_handshake, std::nullopt);
  handshake_run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    SubmittedTabNavigationAndClose_ImmediatelyUpdatesTabCoinsAndReportsRemovedContexts) {
  ASSERT_TRUE(embedded_test_server()->Start());
  const GURL url1 = embedded_test_server()->GetURL("/title1.html");
  const GURL url2 = embedded_test_server()->GetURL("/title2.html");

  // Open two additional tabs so we can test navigating one and closing the
  // other while keeping `web_contents_` alive as the extension host.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* tab1 = TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(tab1, nullptr);
  const int32_t tab1_handle_id = tab1->GetHandle().raw_value();
  const SessionID tab1_session_id =
      sessions::SessionTabHelper::IdForTab(tab1->GetContents());

  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), url1, WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  tabs::TabInterface* tab2 = TabListInterface::From(browser())->GetActiveTab();
  ASSERT_NE(tab2, nullptr);
  const int32_t tab2_handle_id = tab2->GetHandle().raw_value();
  const SessionID tab2_session_id =
      sessions::SessionTabHelper::IdForTab(tab2->GetContents());

  auto* user_data =
      ContextualTasksWebContentsUserData::FromWebContents(web_contents_);
  ASSERT_NE(user_data, nullptr);
  user_data->RecordTabIdMapping(tab1_handle_id, tab1_session_id.id());
  user_data->RecordTabIdMapping(tab2_handle_id, tab2_session_id.id());

  // Attach and submit both tabs on Turn 1.
  base::UnguessableToken token1 = base::UnguessableToken::Create();
  base::UnguessableToken token2 = base::UnguessableToken::Create();
  mock_session_handle_->GetUploadedContextTokensForTesting().push_back(token1);
  mock_session_handle_->GetUploadedContextTokensForTesting().push_back(token2);
  mock_session_handle_->AttachTabPendingUpload(token1, tab1_session_id.id(),
                                               url1, "Title 1");
  mock_session_handle_->AttachTabPendingUpload(token2, tab2_session_id.id(),
                                               url1, "Title 1");

  contextual_search::FileInfo file_info1;
  file_info1.file_token = token1;
  file_info1.tab_session_id = tab1_session_id;
  file_info1.tab_url = url1;
  lens::LensOverlayRequestId req_id1;
  req_id1.set_uuid(1001);
  file_info1.request_id = req_id1;

  contextual_search::FileInfo file_info2;
  file_info2.file_token = token2;
  file_info2.tab_session_id = tab2_session_id;
  file_info2.tab_url = url1;
  lens::LensOverlayRequestId req_id2;
  req_id2.set_uuid(1002);
  file_info2.request_id = req_id2;

  ON_CALL(*mock_controller_, GetFileInfo(token1))
      .WillByDefault(Return(&file_info1));
  ON_CALL(*mock_controller_, GetFileInfo(token2))
      .WillByDefault(Return(&file_info2));
  ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(Return(
          std::vector<contextual_search::FileInfo>{file_info1, file_info2}));

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
        handler_->OnWebviewMessage(serialized_message, std::nullopt);
        run_loop.Run();
        mock_page_.FlushForTesting();
        testing::Mock::VerifyAndClearExpectations(&mock_page_);
      };

  submit(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage::OnSubmitQueryResponse& response) {
        ASSERT_EQ(2, response.added_contexts_size());
        EXPECT_EQ(0, response.removed_contexts_size());
      }));
  ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
      .WillByDefault(Return(std::vector<contextual_search::FileInfo>{}));
  ASSERT_EQ(2u, mock_session_handle_->GetTabContextState().attached.size());

  // 1. Navigate `tab1` to `url2`. `OnTabContextUpdated` must fire immediately
  // with only `tab2` remaining in the tab coin list.
  {
    base::RunLoop tab_context_loop;
    std::vector<int32_t> updated_tab_ids;
    EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
        .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                      const std::vector<int32_t>& submitted_ids) {
          for (const auto& t : tabs) {
            updated_tab_ids.push_back(t->tab_id);
          }
          tab_context_loop.Quit();
        });
    ASSERT_TRUE(content::NavigateToURL(tab1->GetContents(), url2));
    tab_context_loop.Run();
    EXPECT_THAT(updated_tab_ids, testing::ElementsAre(tab2_handle_id));
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

  // Submitting Turn 2 must report `req_id1` (1001) in `removed_contexts` and
  // mark `file_info1` superseded.
  submit(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage::OnSubmitQueryResponse& response) {
        EXPECT_EQ(0, response.added_contexts_size());
        ASSERT_EQ(1, response.removed_contexts_size());
        EXPECT_EQ(1001u, response.removed_contexts(0).request_id().uuid());
      }));
  EXPECT_TRUE(file_info1.is_superceded);

  // Navigating `tab1` back to `url1` after Turn 2 must not resurrect its
  // context or cause it to be recontextualized on Turn 3.
  ASSERT_TRUE(content::NavigateToURL(tab1->GetContents(), url1));

  // 2. Close `tab2`. `OnTabContextUpdated` must fire immediately with an empty
  // tab coin list.
  {
    base::RunLoop tab_context_loop;
    size_t remaining_tabs = 99;
    EXPECT_CALL(mock_page_, OnTabContextUpdated(_, _))
        .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs,
                      const std::vector<int32_t>& submitted_ids) {
          remaining_tabs = tabs.size();
          tab_context_loop.Quit();
        });
    TabListInterface::From(browser())->CloseTab(tab2->GetHandle());
    tab_context_loop.Run();
    EXPECT_EQ(0u, remaining_tabs);
    testing::Mock::VerifyAndClearExpectations(&mock_page_);
  }

  // Submitting Turn 3 must report `req_id2` (1002) in `removed_contexts` and
  // mark `file_info2` superseded, without re-adding `tab1`.
  submit(base::BindLambdaForTesting(
      [&](const lens::ClientToSearchMessage::OnSubmitQueryResponse& response) {
        EXPECT_EQ(0, response.added_contexts_size());
        ASSERT_EQ(1, response.removed_contexts_size());
        EXPECT_EQ(1002u, response.removed_contexts(0).request_id().uuid());
      }));
  EXPECT_TRUE(file_info2.is_superceded);
}

}  // namespace contextual_tasks
