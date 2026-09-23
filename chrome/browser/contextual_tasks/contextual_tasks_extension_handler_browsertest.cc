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
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_page.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
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
#include "mojo/public/cpp/bindings/remote.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
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
  }

  void TearDownOnMainThread() override {
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
  std::string data_id = model1->lens_crop()->data_id;

  base::RunLoop run_loop;
  child_handler->GetLensCropPreview(
      data_id, base::BindLambdaForTesting(
                   [&](const std::optional<std::string>& data_uri) {
                     ASSERT_TRUE(data_uri.has_value());
                     EXPECT_EQ("data:image/png;base64,test_crop", *data_uri);
                     run_loop.Quit();
                   }));
  run_loop.Run();
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

}  // namespace contextual_tasks
