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
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_web_contents_user_data.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_page.h"
#include "chrome/browser/profiles/profile.h"
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
#include "components/contextual_tasks/public/features.h"
#include "components/lens/lens_overlay_dismissal_source.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/permissions/request_type.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/navigation_controller.h"
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
#include "third_party/lens_server_proto/lens_overlay_contextual_inputs.pb.h"
#include "third_party/lens_server_proto/lens_overlay_visual_search_interaction_data.pb.h"
#include "third_party/lens_server_proto/search_communication.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "ui/base/unowned_user_data/user_data_factory.h"

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
                  this->mock_lens_controller_ = mock.get();
                  return mock;
                }));
  }
  ~ContextualTasksExtensionHandlerBrowserTestBase() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
    content::RenderFrameHost* rfh = web_contents_->GetPrimaryMainFrame();

    ContextualTasksExtensionHandler::CreateForCurrentDocument(rfh);
    handler_ = ContextualTasksExtensionHandler::GetForCurrentDocument(rfh);
    ASSERT_NE(handler_, nullptr);

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

    // Set up mock session handle and controller.
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

    ON_CALL(*mock_session_handle_, GetUploadedContextFileInfos())
        .WillByDefault(Return(file_infos));

    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents_)
        ->SetTaskSession(std::nullopt, std::move(session_handle),
                         /*input_state_model=*/nullptr);

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
    mock_session_handle_ = nullptr;
    mock_controller_.reset();
    web_contents_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  ui::UserDataFactory::ScopedOverride lens_controller_override_;
  raw_ptr<MockLensSearchController> mock_lens_controller_ = nullptr;
  std::unique_ptr<NiceMock<MockLensOverlayController>>
      mock_lens_overlay_controller_;
  base::test::ScopedFeatureList feature_list_;
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<ContextualTasksExtensionHandler> handler_ = nullptr;
  NiceMock<MockContextualTasksExtensionPage> mock_page_;
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
            {kContextualTasks, kContextualTasksRearchitecture},
            {}) {}
};

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       SubmitQuery) {
  base::RunLoop run_loop;

  // We expect PostAimMessage to be called on the mock page.
  EXPECT_CALL(mock_page_, PostAimMessage(_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  // Call SubmitQuery on the handler (which implements
  // searchbox::mojom::PageHandler). We cast it to make sure we are calling the
  // interface method.
  static_cast<searchbox::mojom::PageHandler*>(handler_)->SubmitQuery(
      "test query", 0, false, false, false, false, /*is_voice_search=*/false);

  run_loop.Run();
}

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

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    HandleLensButtonClick_OverlayOpenFromComposebox_ClosesOverlay) {
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
  base::RunLoop run_loop;
  static_cast<searchbox::mojom::PageHandler*>(handler_)->AddTabContext(
      1, /*delay_upload=*/false,
      searchbox::mojom::TabAttachmentSource::kContextMenu,
      base::BindLambdaForTesting(
          [&](base::expected<base::UnguessableToken,
                             contextual_search::ContextUploadErrorType>
                  result) {
            EXPECT_TRUE(result.has_value());
            run_loop.Quit();
          }));
  run_loop.Run();

  static_cast<searchbox::mojom::PageHandler*>(handler_)->DeleteTabContext(1);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksExtensionHandlerBrowserTest,
                       TwoFramesResolveSameInputStateModel) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  handler_->SetTaskId(task_id);

  auto model1 = handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model1);

  // Set a lens crop through the primary handler.
  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,test_crop");

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
  // with is_active=false to AIM.
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
        EXPECT_EQ(0, response.added_contexts_size());
        run_loop.Quit();
      });

  lens::SearchToClientMessage request;
  request.mutable_on_submit_query_request();
  const size_t size = request.ByteSizeLong();
  std::vector<uint8_t> serialized_message(size);
  request.SerializeToArray(serialized_message.data(), size);

  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();
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

  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();
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

  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksExtensionHandlerBrowserTest,
    OnWebviewMessage_OnSubmitQueryRequest_WithoutRegion_ReturnsEmptyResponse) {
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

  handler_->OnWebviewMessage(serialized_message);
  run_loop.Run();
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
    OnLensOverlayStateChanged_ReverseSyncClearsModelAndEmitsUnmount) {
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
        run_loop.Quit();
      });
  handler_->OnLensThumbnailCreatedForTesting("data:image/png;base64,test_crop");
  run_loop.Run();

  auto model = handler_->GetOrCreateInputStateModelForTesting();
  ASSERT_TRUE(model);
  EXPECT_TRUE(model->GetLensCrop().has_value());

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
        run_loop2.Quit();
      });

  handler_->OnLensOverlayStateChanged(false);
  run_loop2.Run();

  EXPECT_FALSE(model->GetLensCrop().has_value());
  EXPECT_FALSE(model->lens_crop().has_value());
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
  GURL chip_url = embedded_test_server()->GetURL("/lens_chip.html");
  ASSERT_TRUE(
      content::ExecJs(web_contents_,
                      "const iframe = document.createElement('iframe'); "
                      "iframe.id = 'chip_iframe'; "
                      "iframe.name = 'chip_iframe'; "
                      "document.body.appendChild(iframe);"));
  ASSERT_TRUE(
      content::NavigateIframeToURL(web_contents_, "chip_iframe", chip_url));

  content::RenderFrameHost* child_rfh =
      content::ChildFrameAt(web_contents_->GetPrimaryMainFrame(), 0);
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

}  // namespace contextual_tasks
