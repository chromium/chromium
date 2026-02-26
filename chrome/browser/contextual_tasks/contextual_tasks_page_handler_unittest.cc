// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/global_features.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/lens/lens_url_utils.h"
#include "content/public/test/test_web_ui.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
namespace contextual_tasks {

using testing::_;
using testing::NiceMock;
using testing::Return;

constexpr char kAiPageUrl[] = "https://google.com/search?udm=50";
constexpr char kQueryUrl[] = "https://google.com/search?q=test";
constexpr char kThreadUrl[] = "https://google.com/search?mtid=123";
constexpr char kExampleUrl[] = "https://example.com";
constexpr char kExamplePdfUrl[] = "https://example.com/file.pdf";

class MockPage : public mojom::Page {
 public:
  MockPage() = default;
  ~MockPage() override = default;

  mojo::PendingRemote<mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

  MOCK_METHOD(void, SetThreadTitle, (const std::string& title), (override));
  MOCK_METHOD(void,
              SetTaskDetails,
              (const base::Uuid& uuid,
               const std::string& thread_id,
               const std::string& turn_id),
              (override));
  MOCK_METHOD(void, SetAimUrl, (const GURL& url), (override));
  MOCK_METHOD(void, OnSidePanelStateChanged, (), (override));
  MOCK_METHOD(void,
              PostMessageToWebview,
              (const std::vector<uint8_t>& message),
              (override));
  MOCK_METHOD(void, OnHandshakeComplete, (), (override));
  MOCK_METHOD(void,
              SetOAuthToken,
              (const std::string& oauth_token),
              (override));
  MOCK_METHOD(void,
              OnContextUpdated,
              (std::vector<mojom::ContextInfoPtr> context),
              (override));
  MOCK_METHOD(void, HideInput, (), (override));
  MOCK_METHOD(void, RestoreInput, (), (override));
  MOCK_METHOD(void, OnZeroStateChange, (bool is_zero_state), (override));
  MOCK_METHOD(void, OnAiPageStatusChanged, (bool is_ai_page), (override));
  MOCK_METHOD(void,
              OnLensOverlayStateChanged,
              (bool is_showing, bool maybe_show_overlay_hint_text),
              (override));
  MOCK_METHOD(void, ShowErrorPage, (), (override));
  MOCK_METHOD(void, HideErrorPage, (), (override));
  MOCK_METHOD(void, ShowOauthErrorDialog, (), (override));
  MOCK_METHOD(void,
              UpdateComposeboxPosition,
              (mojom::ComposeboxPositionPtr position),
              (override));
  MOCK_METHOD(void, LockInput, (), (override));
  MOCK_METHOD(void, UnlockInput, (), (override));
  MOCK_METHOD(void,
              InjectInput,
              (const std::string& title,
               const std::string& thumbnail,
               const base::UnguessableToken& file_token),
              (override));
  MOCK_METHOD(void,
              RemoveInjectedInput,
              (const base::UnguessableToken& file_token),
              (override));

  mojo::Receiver<mojom::Page> receiver_{this};
};

class MockUiService : public ContextualTasksUiService {
 public:
  MockUiService(Profile* profile, ContextualTasksService* service)
      : ContextualTasksUiService(profile, service, nullptr, nullptr) {}

  MOCK_METHOD(GURL, GetDefaultAiPageUrl, (), (override));
  MOCK_METHOD(GURL,
              GetDefaultAiPageUrlForTask,
              (const base::Uuid& task_id),
              (override));
  MOCK_METHOD(void,
              SetInitialEntryPointForTask,
              (const base::Uuid&, omnibox::ChromeAimEntryPoint),
              (override));
  MOCK_METHOD(std::optional<GURL>,
              GetInitialUrlForTask,
              (const base::Uuid&),
              (override));
  MOCK_METHOD(void,
              GetThreadUrlFromTaskId,
              (const base::Uuid&, base::OnceCallback<void(GURL)>),
              (override));
  MOCK_METHOD(void,
              MoveTaskUiToNewTab,
              (const base::Uuid&, BrowserWindowInterface*, const GURL&),
              (override));
  MOCK_METHOD(void,
              OnTabClickedFromSourcesMenu,
              (int32_t, const GURL&, BrowserWindowInterface*),
              (override));
  MOCK_METHOD(void,
              OnFileClickedFromSourcesMenu,
              (const GURL&, BrowserWindowInterface*),
              (override));
  MOCK_METHOD(void,
              OnImageClickedFromSourcesMenu,
              (const GURL&, BrowserWindowInterface*),
              (override));
  MOCK_METHOD(bool, IsAiUrl, (const GURL&), (override));
  MOCK_METHOD(bool, IsPendingErrorPage, (const base::Uuid&), (override));
};

class TestContextualTasksUI : public ContextualTasksUI {
 public:
  explicit TestContextualTasksUI(content::WebUI* web_ui)
      : ContextualTasksUI(web_ui) {}
  MOCK_METHOD(void,
              PostMessageToWebview,
              (const lens::ClientToAimMessage& message),
              (override));
  MOCK_METHOD(contextual_search::ContextualSearchSessionHandle*,
              GetOrCreateContextualSessionHandle,
              (),
              (override));
};

class ContextualTasksPageHandlerTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    feature_list_.InitAndEnableFeature(kContextualTasksContextLibrary);
    BrowserWithTestWindowTest::SetUp();

    ContextualTasksServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindOnce([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<NiceMock<MockContextualTasksService>>());
        }));

    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindOnce([](content::BrowserContext* context) {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::unique_ptr<KeyedService>(
              std::make_unique<NiceMock<MockUiService>>(
                  profile,
                  ContextualTasksServiceFactory::GetForProfile(profile)));
        }));

    web_contents_ = content::WebContents::Create(
        content::WebContents::CreateParams(profile()));
    web_ui_.set_web_contents(web_contents_.get());
    contextual_tasks_ui_ =
        std::make_unique<NiceMock<TestContextualTasksUI>>(&web_ui_);

    // Bind the mock page to the controller.
    contextual_tasks_ui_->GetPageRemote().Bind(page_.BindAndGetRemote());

    mock_contextual_tasks_service_ = static_cast<MockContextualTasksService*>(
        ContextualTasksServiceFactory::GetForProfile(profile()));
    mock_contextual_tasks_ui_service_ = static_cast<MockUiService*>(
        ContextualTasksUiServiceFactory::GetForBrowserContext(profile()));

    page_handler_ = std::make_unique<ContextualTasksPageHandler>(
        mojo::PendingReceiver<mojom::PageHandler>(), contextual_tasks_ui_.get(),
        mock_contextual_tasks_ui_service_, mock_contextual_tasks_service_);
    page_handler_->set_skip_feedback_ui_for_testing(true);
  }

  void TearDown() override {
    page_handler_.reset();
    contextual_tasks_ui_.reset();
    web_contents_.reset();
    mock_contextual_tasks_service_ = nullptr;
    mock_contextual_tasks_ui_service_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

 protected:
  content::TestWebUI web_ui_;
  std::unique_ptr<content::WebContents> web_contents_;
  std::unique_ptr<NiceMock<TestContextualTasksUI>> contextual_tasks_ui_;
  std::unique_ptr<ContextualTasksPageHandler> page_handler_;
  raw_ptr<MockContextualTasksService> mock_contextual_tasks_service_;
  raw_ptr<MockUiService> mock_contextual_tasks_ui_service_;
  NiceMock<MockPage> page_;
  base::test::ScopedFeatureList feature_list_;
};

TEST_F(ContextualTasksPageHandlerTest, IsPendingErrorPage) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  EXPECT_CALL(*mock_contextual_tasks_ui_service_, IsPendingErrorPage(task_id))
      .WillOnce(Return(true));

  base::RunLoop run_loop;
  page_handler_->IsPendingErrorPage(
      task_id, base::BindLambdaForTesting([&](bool is_pending_error_page) {
        EXPECT_TRUE(is_pending_error_page);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ContextualTasksPageHandlerTest, IsPendingErrorPage_TaskNotPending) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  base::RunLoop run_loop;
  page_handler_->IsPendingErrorPage(
      task_id, base::BindLambdaForTesting([&](bool is_pending_error_page) {
        EXPECT_FALSE(is_pending_error_page);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ContextualTasksPageHandlerTest, GetThreadUrl) {
  GURL expected_url(kAiPageUrl);
  EXPECT_CALL(*mock_contextual_tasks_ui_service_, GetDefaultAiPageUrl())
      .WillOnce(Return(expected_url));

  base::RunLoop run_loop;
  page_handler_->GetThreadUrl(base::BindLambdaForTesting([&](const GURL& url) {
    EXPECT_EQ(url, expected_url);
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(ContextualTasksPageHandlerTest, GetUrlForTask_InitialUrlExists) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  GURL expected_url(kQueryUrl);
  EXPECT_CALL(*mock_contextual_tasks_ui_service_, GetInitialUrlForTask(task_id))
      .WillOnce(Return(expected_url));

  base::RunLoop run_loop;
  page_handler_->GetUrlForTask(task_id,
                               base::BindLambdaForTesting([&](const GURL& url) {
                                 EXPECT_EQ(url, expected_url);
                                 run_loop.Quit();
                               }));
  run_loop.Run();
}

TEST_F(ContextualTasksPageHandlerTest, GetUrlForTask_FetchFromService) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  GURL expected_url(kThreadUrl);

  EXPECT_CALL(*mock_contextual_tasks_ui_service_, GetInitialUrlForTask(task_id))
      .WillOnce(Return(std::nullopt));
  EXPECT_CALL(*mock_contextual_tasks_ui_service_,
              GetThreadUrlFromTaskId(task_id, _))
      .WillOnce(
          [&](const base::Uuid&, base::OnceCallback<void(GURL)> callback) {
            std::move(callback).Run(expected_url);
          });

  base::RunLoop run_loop;
  page_handler_->GetUrlForTask(task_id,
                               base::BindLambdaForTesting([&](const GURL& url) {
                                 EXPECT_EQ(url, expected_url);
                                 run_loop.Quit();
                               }));
  run_loop.Run();
}

TEST_F(ContextualTasksPageHandlerTest,
       OnWebviewMessage_ResizeComposeboxPosition) {
  lens::AimToClientMessage message;
  auto* update_params =
      message.mutable_set_chrome_desktop_input_plate_configuration();
  update_params->set_max_width(500);
  update_params->set_max_height(600);
  update_params->set_margin_left(70);
  update_params->set_margin_bottom(-80);

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);
  EXPECT_CALL(page_,
              UpdateComposeboxPosition(testing::Pointee(testing::AllOf(
                  testing::Field(&mojom::ComposeboxPosition::max_width,
                                 update_params->max_width()),
                  testing::Field(&mojom::ComposeboxPosition::max_height,
                                 update_params->max_height()),
                  testing::Field(&mojom::ComposeboxPosition::margin_bottom,
                                 update_params->margin_bottom()),
                  testing::Field(&mojom::ComposeboxPosition::margin_left,
                                 update_params->margin_left())))))
      .Times(1);

  page_handler_->OnWebviewMessage(serialized);
}

TEST_F(ContextualTasksPageHandlerTest,
       OnWebviewMessage_ResizeComposeboxPosition_MaxValues) {
  lens::AimToClientMessage message;
  auto* update_params =
      message.mutable_set_chrome_desktop_input_plate_configuration();
  update_params->set_max_width(INT32_MAX);
  update_params->set_max_height(INT32_MAX);
  update_params->set_margin_left(INT32_MAX);
  update_params->set_margin_bottom(INT32_MAX);
  auto composebox_position =
      contextual_tasks::InputPlateConfigToMojo(*update_params);
  EXPECT_EQ(composebox_position->max_width, INT32_MAX);
  EXPECT_EQ(composebox_position->max_height, INT32_MAX);
  EXPECT_EQ(composebox_position->margin_left, INT32_MAX);
  EXPECT_EQ(composebox_position->margin_bottom, INT32_MAX);
}

TEST_F(ContextualTasksPageHandlerTest,
       OnWebviewMessage_ResizeComposeboxPosition_NegativeMinValues) {
  lens::AimToClientMessage message;
  auto* update_params =
      message.mutable_set_chrome_desktop_input_plate_configuration();
  update_params->set_max_width(INT32_MIN);
  update_params->set_max_height(INT32_MIN);
  update_params->set_margin_left(INT32_MIN);
  update_params->set_margin_bottom(INT32_MIN);
  auto composebox_position =
      contextual_tasks::InputPlateConfigToMojo(*update_params);
  EXPECT_EQ(composebox_position->max_width, 0);
  EXPECT_EQ(composebox_position->max_height, 0);
  EXPECT_EQ(composebox_position->margin_left, INT32_MIN);
  EXPECT_EQ(composebox_position->margin_bottom, INT32_MIN);
}

TEST_F(ContextualTasksPageHandlerTest,
       OnWebviewMessage_PartiallyResizeComposeboxPosition) {
  lens::AimToClientMessage message;
  auto* update_params =
      message.mutable_set_chrome_desktop_input_plate_configuration();
  update_params->set_margin_left(-70);

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);
  EXPECT_CALL(page_,
              UpdateComposeboxPosition(testing::Pointee(testing::AllOf(
                  testing::Field(&mojom::ComposeboxPosition::max_height,
                                 testing::Eq(std::nullopt)),
                  testing::Field(&mojom::ComposeboxPosition::max_width,
                                 testing::Eq(std::nullopt)),
                  testing::Field(&mojom::ComposeboxPosition::margin_bottom,
                                 testing::Eq(std::nullopt)),
                  testing::Field(&mojom::ComposeboxPosition::margin_left,
                                 update_params->margin_left())))))
      .Times(1);

  page_handler_->OnWebviewMessage(serialized);
}

TEST_F(ContextualTasksPageHandlerTest,
       OnWebviewMessage_ResizeComposeboxPositionOptional) {
  lens::AimToClientMessage message;

  message.mutable_set_chrome_desktop_input_plate_configuration();

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);
  EXPECT_CALL(page_,
              UpdateComposeboxPosition(testing::Pointee(testing::AllOf(
                  testing::Field(&mojom::ComposeboxPosition::max_height,
                                 testing::Eq(std::nullopt)),
                  testing::Field(&mojom::ComposeboxPosition::max_width,
                                 testing::Eq(std::nullopt)),
                  testing::Field(&mojom::ComposeboxPosition::margin_bottom,
                                 testing::Eq(std::nullopt)),
                  testing::Field(&mojom::ComposeboxPosition::margin_left,
                                 testing::Eq(std::nullopt))))))
      .Times(1);

  page_handler_->OnWebviewMessage(serialized);
}

TEST_F(ContextualTasksPageHandlerTest,
       OnWebviewMessage_DistinguishesZeroFromUnset) {
  lens::AimToClientMessage message;
  auto* update_params =
      message.mutable_set_chrome_desktop_input_plate_configuration();

  update_params->set_max_width(0);

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);

  EXPECT_CALL(page_,
              UpdateComposeboxPosition(testing::Pointee(testing::AllOf(
                  testing::Field(&mojom::ComposeboxPosition::max_width,
                                 testing::Optional(0)),
                  testing::Field(&mojom::ComposeboxPosition::max_height,
                                 testing::Eq(std::nullopt)),
                  testing::Field(&mojom::ComposeboxPosition::margin_bottom,
                                 testing::Eq(std::nullopt)),
                  testing::Field(&mojom::ComposeboxPosition::margin_left,
                                 testing::Eq(std::nullopt))))))
      .Times(1);

  page_handler_->OnWebviewMessage(serialized);
}

TEST_F(ContextualTasksPageHandlerTest, OnWebviewMessage_IgnoreMalformedData) {
  std::vector<uint8_t> garbage_data = {0xDE, 0xAD, 0xBE, 0xEF};

  EXPECT_CALL(page_, UpdateComposeboxPosition(testing::_)).Times(0);

  page_handler_->OnWebviewMessage(garbage_data);
}

TEST_F(ContextualTasksPageHandlerTest, SetTaskId) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  // SetTaskId calls contextual_tasks_ui_->SetTaskId() and then
  // UpdateContextForTask(). UpdateContextForTask calls
  // contextual_tasks_service_->GetContextForTask().

  EXPECT_CALL(*mock_contextual_tasks_service_,
              GetContextForTask(task_id, _, _, _))
      .Times(1);

  page_handler_->SetTaskId(task_id);
  EXPECT_EQ(contextual_tasks_ui_->GetTaskId(), task_id);
}

TEST_F(ContextualTasksPageHandlerTest, SetThreadTitle) {
  std::string title = "New Thread Title";
  page_handler_->SetThreadTitle(title);
  EXPECT_EQ(contextual_tasks_ui_->GetThreadTitle(), title);
}

TEST_F(ContextualTasksPageHandlerTest, IsZeroState) {
  GURL url(kAiPageUrl);

  // IsZeroState in ContextualTasksUI uses ui_service to check if it's an AI
  // URL.
  EXPECT_CALL(*mock_contextual_tasks_ui_service_, IsAiUrl(url))
      .WillRepeatedly(Return(true));

  base::RunLoop run_loop;
  page_handler_->IsZeroState(
      url, base::BindLambdaForTesting([&](bool is_zero_state) {
        EXPECT_TRUE(is_zero_state);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ContextualTasksPageHandlerTest, IsShownInTab) {
  // Our setup uses a WebContents not yet attached to a tab interface in a way
  // that IsShownInTab() would return true easily without more setup.
  // But we can test that it calls the controller.
  base::RunLoop run_loop;
  page_handler_->IsShownInTab(base::BindLambdaForTesting([&](bool is_in_tab) {
    EXPECT_FALSE(is_in_tab);  // Should be false for our TestWebUI setup
    run_loop.Quit();
  }));
  run_loop.Run();
}

TEST_F(ContextualTasksPageHandlerTest, CloseSidePanel) {
  // CloseSidePanel calls contextual_tasks_ui_->CloseSidePanel().
  // We can't easily check side panel state here, but we can verify it doesn't
  // crash.
  page_handler_->CloseSidePanel();
}

TEST_F(ContextualTasksPageHandlerTest, ShowThreadHistory) {
  // ShowThreadHistory sends a message to the webview.
  EXPECT_CALL(page_, PostMessageToWebview(_))
      .WillOnce([&](const std::vector<uint8_t>& message) {
        lens::ClientToAimMessage client_message;
        ASSERT_TRUE(
            client_message.ParseFromArray(message.data(), message.size()));
        EXPECT_TRUE(client_message.has_open_threads_view());
      });

  page_handler_->ShowThreadHistory();
}

TEST_F(ContextualTasksPageHandlerTest, MoveTaskUiToNewTab) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  contextual_tasks_ui_->SetTaskId(task_id);

  EXPECT_CALL(*mock_contextual_tasks_ui_service_,
              MoveTaskUiToNewTab(task_id, _, _))
      .Times(1);

  page_handler_->MoveTaskUiToNewTab();
}

TEST_F(ContextualTasksPageHandlerTest, OnTabClickedFromSourcesMenu) {
  int32_t tab_id = 123;
  GURL url(kExampleUrl);

  EXPECT_CALL(*mock_contextual_tasks_ui_service_,
              OnTabClickedFromSourcesMenu(tab_id, url, _))
      .Times(1);

  page_handler_->OnTabClickedFromSourcesMenu(tab_id, url);
}

TEST_F(ContextualTasksPageHandlerTest, OnFileClickedFromSourcesMenu) {
  GURL url(kExamplePdfUrl);

  EXPECT_CALL(*mock_contextual_tasks_ui_service_,
              OnFileClickedFromSourcesMenu(url, _))
      .Times(1);

  page_handler_->OnFileClickedFromSourcesMenu(url);
}

TEST_F(ContextualTasksPageHandlerTest, OnImageClickedFromSourcesMenu) {
  GURL url(kExampleUrl);

  EXPECT_CALL(*mock_contextual_tasks_ui_service_,
              OnImageClickedFromSourcesMenu(url, _))
      .Times(1);

  page_handler_->OnImageClickedFromSourcesMenu(url);
}

TEST_F(ContextualTasksPageHandlerTest, OnWebviewMessage_HandshakeResponse) {
  lens::AimToClientMessage message;
  message.mutable_handshake_response();

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);

  EXPECT_CALL(page_, OnHandshakeComplete()).Times(1);
  EXPECT_CALL(page_, OnSidePanelStateChanged()).Times(1);

  page_handler_->OnWebviewMessage(serialized);
}

TEST_F(ContextualTasksPageHandlerTest, OnWebviewMessage_HideInput) {
  lens::AimToClientMessage message;
  message.mutable_hide_input();

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);

  EXPECT_CALL(page_, HideInput()).Times(1);

  page_handler_->OnWebviewMessage(serialized);
}

TEST_F(ContextualTasksPageHandlerTest, OnWebviewMessage_RestoreInput) {
  lens::AimToClientMessage message;
  message.mutable_restore_input();

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);

  EXPECT_CALL(page_, RestoreInput()).Times(1);

  page_handler_->OnWebviewMessage(serialized);
}

TEST_F(ContextualTasksPageHandlerTest,
       OnWebviewMessage_NotifyZeroStateRendered) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableNotifyZeroStateRenderedCapability);

  lens::AimToClientMessage message;
  message.mutable_notify_zero_state_rendered()->set_is_zero_state_rendered(
      true);

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);

  EXPECT_CALL(page_, OnZeroStateChange(true)).Times(1);

  page_handler_->OnWebviewMessage(serialized);
}

TEST_F(ContextualTasksPageHandlerTest,
       OnWebviewMessage_NotifyZeroStateRendered_False) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableNotifyZeroStateRenderedCapability);

  lens::AimToClientMessage message;
  message.mutable_notify_zero_state_rendered()->set_is_zero_state_rendered(
      false);

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);

  EXPECT_CALL(page_, OnZeroStateChange(false)).Times(1);

  page_handler_->OnWebviewMessage(serialized);
}

TEST_F(ContextualTasksPageHandlerTest, OpenMyActivityUi) {
  // Smoke test to ensure it doesn't crash.
  page_handler_->OpenMyActivityUi();
}

TEST_F(ContextualTasksPageHandlerTest, OpenHelpUi) {
  // Smoke test to ensure it doesn't crash.
  page_handler_->OpenHelpUi();
}

TEST_F(ContextualTasksPageHandlerTest, OpenOnboardingHelpUi) {
  // Smoke test to ensure it doesn't crash.
  page_handler_->OpenOnboardingHelpUi();
}

TEST_F(ContextualTasksPageHandlerTest, OnboardingTooltipDismissed) {
  // Smoke test to ensure it doesn't crash.
  page_handler_->OnboardingTooltipDismissed();
}

TEST_F(ContextualTasksPageHandlerTest, GetCommonSearchParams) {
  g_browser_process->GetFeatures()->application_locale_storage()->Set("en-US");

  // Default case: side panel enabled, dark mode disabled.
  {
    base::RunLoop run_loop;
    page_handler_->GetCommonSearchParams(
        /*is_dark_mode=*/false, /*is_side_panel=*/true,
        base::BindLambdaForTesting(
            [&](const base::flat_map<std::string, std::string>& params) {
              EXPECT_EQ(params.at(lens::kLanguageCodeParameterKey), "en-US");
              EXPECT_EQ(params.at(lens::kDarkModeParameterKey),
                        lens::kDarkModeParameterLightValue);
              EXPECT_EQ(params.at(lens::kChromeSidePanelParameterKey), "2");
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Dark mode enabled, side panel disabled.
  {
    base::RunLoop run_loop;
    page_handler_->GetCommonSearchParams(
        /*is_dark_mode=*/true, /*is_side_panel=*/false,
        base::BindLambdaForTesting(
            [&](const base::flat_map<std::string, std::string>& params) {
              EXPECT_EQ(params.at(lens::kLanguageCodeParameterKey), "en-US");
              EXPECT_EQ(params.at(lens::kDarkModeParameterKey),
                        lens::kDarkModeParameterDarkValue);
              EXPECT_EQ(params.at(lens::kChromeSidePanelParameterKey), "");
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Test with kContextualTasksForceCountryCodeUS enabled.
  {
    base::test::ScopedFeatureList country_feature_list;
    country_feature_list.InitAndEnableFeature(
        kContextualTasksForceCountryCodeUS);

    base::RunLoop run_loop;
    page_handler_->GetCommonSearchParams(
        /*is_dark_mode=*/false, /*is_side_panel=*/true,
        base::BindLambdaForTesting(
            [&](const base::flat_map<std::string, std::string>& params) {
              EXPECT_EQ(params.at(lens::kLanguageCodeParameterKey), "US");
              EXPECT_EQ(params.at("gl"), "us");
              run_loop.Quit();
            }));
    run_loop.Run();
  }

  // Test with ForceGscInTabMode enabled.
  {
    base::test::ScopedFeatureList gsc_feature_list;
    gsc_feature_list.InitAndEnableFeatureWithParameters(
        kContextualTasks, {{"ContextualTasksForceGscInTabMode", "true"}});

    base::RunLoop run_loop;
    page_handler_->GetCommonSearchParams(
        /*is_dark_mode=*/false, /*is_side_panel=*/false,
        base::BindLambdaForTesting(
            [&](const base::flat_map<std::string, std::string>& params) {
              // Should be "2" even if is_side_panel was false because of the
              // force flag.
              EXPECT_EQ(params.at(lens::kChromeSidePanelParameterKey), "2");
              run_loop.Quit();
            }));
    run_loop.Run();
  }
}

TEST_F(ContextualTasksPageHandlerTest,
       OnWebviewMessage_UpdateThreadContextLibrary) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  contextual_tasks_ui_->SetTaskId(task_id);

  lens::AimToClientMessage message;
  auto* update = message.mutable_update_thread_context_library();
  auto* context = update->add_contexts();
  context->set_context_id(123);
  context->mutable_webpage()->set_url(kExampleUrl);

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);

  EXPECT_CALL(*contextual_tasks_ui_, GetOrCreateContextualSessionHandle())
      .WillOnce(Return(nullptr));
  EXPECT_CALL(*mock_contextual_tasks_service_,
              SetUrlResourcesFromServer(task_id, _))
      .Times(1);

  page_handler_->OnWebviewMessage(serialized);
}

TEST_F(ContextualTasksPageHandlerTest, PostMessageToWebview) {
  lens::ClientToAimMessage message;
  message.mutable_open_threads_view();

  EXPECT_CALL(page_, PostMessageToWebview(_)).Times(1);
  page_handler_->PostMessageToWebview(message);
}

TEST_F(ContextualTasksPageHandlerTest, OnTaskUpdated) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  contextual_tasks_ui_->SetTaskId(task_id);

  ContextualTask task(task_id);

  EXPECT_CALL(*mock_contextual_tasks_service_,
              GetContextForTask(task_id, _, _, _))
      .Times(1);

  page_handler_->OnTaskUpdated(task,
                               ContextualTasksService::TriggerSource::kLocal);
}

TEST_F(ContextualTasksPageHandlerTest,
       OnContextUpdated_TabsImagesAndFiles_WithFiltering) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  contextual_tasks_ui_->SetTaskId(task_id);

  ContextualTask task(task_id);
  // Valid items
  UrlResource tab_resource(GURL(kQueryUrl), ResourceType::kWebpage);
  tab_resource.title = "Example Tab";
  tab_resource.tab_id = SessionID::NewUnique();
  task.AddUrlResource(tab_resource);

  UrlResource image_resource(GURL(kExampleUrl), ResourceType::kImage);
  image_resource.title = "Example Image";
  task.AddUrlResource(image_resource);

  UrlResource pdf_resource(GURL(kExamplePdfUrl), ResourceType::kPdf);
  pdf_resource.title = "Example PDF";
  task.AddUrlResource(pdf_resource);

  // Invalid items to be filtered.
  UrlResource empty_pdf_url_resource(GURL(), ResourceType::kPdf);
  empty_pdf_url_resource.title = "Valid PDF with empty URL";
  task.AddUrlResource(empty_pdf_url_resource);

  UrlResource empty_url_resource(GURL(""), ResourceType::kWebpage);
  empty_url_resource.title = "Tab with empty URL";
  task.AddUrlResource(empty_url_resource);

  UrlResource empty_title_resource(GURL(kExampleUrl), ResourceType::kWebpage);
  empty_title_resource.title = "";
  task.AddUrlResource(empty_title_resource);

  EXPECT_CALL(*mock_contextual_tasks_service_,
              GetContextForTask(task_id, _, _, _))
      .WillOnce(
          [&](const base::Uuid&, const std::set<ContextualTaskContextSource>&,
              std::unique_ptr<ContextDecorationParams>,
              base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
                  callback) {
            std::move(callback).Run(

                std::make_unique<ContextualTaskContext>(task));
          });

  base::RunLoop run_loop;
  EXPECT_CALL(page_, OnContextUpdated(_))
      .WillOnce([&](std::vector<mojom::ContextInfoPtr> context) {
        // Only the first 3 valid items should be present.
        ASSERT_EQ(context.size(), 4u);

        EXPECT_TRUE(context[0]->is_tab());
        EXPECT_EQ(context[0]->get_tab()->title, tab_resource.title);
        EXPECT_EQ(context[0]->get_tab()->url, GURL(kQueryUrl));
        EXPECT_EQ(context[0]->get_tab()->tab_id, tab_resource.tab_id->id());

        EXPECT_TRUE(context[1]->is_image());
        EXPECT_EQ(context[1]->get_image()->title, image_resource.title);
        EXPECT_EQ(context[1]->get_image()->url, GURL(kExampleUrl));

        EXPECT_TRUE(context[2]->is_file());
        EXPECT_EQ(context[2]->get_file()->title, pdf_resource.title);
        EXPECT_EQ(context[2]->get_file()->url, GURL(kExamplePdfUrl));

        EXPECT_TRUE(context[3]->is_file());
        EXPECT_EQ(context[3]->get_file()->title, empty_pdf_url_resource.title);
        EXPECT_EQ(context[3]->get_file()->url, GURL());

        run_loop.Quit();
      });

  page_handler_->OnTaskUpdated(task,
                               ContextualTasksService::TriggerSource::kLocal);
  run_loop.Run();
}

}  // namespace contextual_tasks
