// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_page_handler.h"

#include <cstdint>
#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/browser_process.h"
#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/ui/actions/chrome_action_id.h"  // nogncheck
#include "chrome/browser/ui/actions/chrome_actions.h"  // nogncheck
#include "chrome/browser/ui/browser_actions.h"  // nogncheck
#include "chrome/browser/ui/toolbar/pinned_toolbar/pinned_toolbar_actions_model.h"  // nogncheck
#include "ui/actions/actions.h"  // nogncheck
#endif
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_page.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_ui_service.h"
#include "chrome/browser/global_features.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/tab_list/mock_tab_list_interface.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/application_locale_storage/application_locale_storage.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/contextual_tasks/public/query_contextualizer.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/lens/lens_url_utils.h"
#include "components/prefs/pref_service.h"
#include "components/tab_groups/tab_group_visual_data.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/test_web_ui.h"
#include "content/public/test/web_contents_tester.h"
#include "net/base/url_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "ui/base/unowned_user_data/unowned_user_data_host.h"
#include "ui/gfx/range/range.h"

namespace contextual_tasks {

using testing::_;
using testing::NiceMock;
using testing::Return;
using testing::ReturnRef;

#if !BUILDFLAG(IS_ANDROID)
using ::kActionSidePanelShowContextualTasks;
#endif

constexpr char kAiPageUrl[] = "https://google.com/search?udm=50";
constexpr char kQueryUrl[] = "https://google.com/search?q=test";
constexpr char kThreadUrl[] = "https://google.com/search?mtid=123";
constexpr char kExampleUrl[] = "https://example.com";
constexpr char kExamplePdfUrl[] = "https://example.com/file.pdf";

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
  MOCK_METHOD(GURL, GetWebUiUrl, (), (override));
  MOCK_METHOD(content::WebContents*, GetInnerWebContents, (), (const, override));
  MOCK_METHOD(BrowserWindowInterface*, GetBrowser, (), (override));
};

class MockContextualTasksUiServiceForThreadLink
    : public MockContextualTasksUiService {
 public:
  MockContextualTasksUiServiceForThreadLink(
      Profile* profile,
      ContextualTasksService* service,
      signin::IdentityManager* identity_manager,
      AimEligibilityService* aim_eligibility_service,
      std::unique_ptr<ContextualTasksEligibilityManager> eligibility_manager,
      std::unique_ptr<ContextualTasksCookieSynchronizer> cookie_synchronizer)
      : MockContextualTasksUiService(profile,
                                     service,
                                     identity_manager,
                                     aim_eligibility_service,
                                     std::move(eligibility_manager),
                                     std::move(cookie_synchronizer)) {}
  ~MockContextualTasksUiServiceForThreadLink() override = default;

  MOCK_METHOD(void,
              OnThreadLinkClicked,
              (const GURL& url,
               base::Uuid task_id,
               base::WeakPtr<tabs::TabInterface> tab,
               base::WeakPtr<BrowserWindowInterface> browser,
               const url::Origin& initiator_origin),
              (override));
};

class ContextualTasksPageHandlerTest : public ChromeRenderViewHostTestHarness {
 public:
  void SetUp() override {
    feature_list_.InitWithFeatures(
        {kContextualTasksContextLibrary,
         kEnableContextualTasksPinButtonInToolbar},
        {});
#if !BUILDFLAG(IS_ANDROID)
    InitializeActionIdStringMapping();
#endif
    profile_manager_ = std::make_unique<TestingProfileManager>(
        TestingBrowserProcess::GetGlobal());
    ASSERT_TRUE(profile_manager_->SetUp());
    ChromeRenderViewHostTestHarness::SetUp();

    ContextualTasksServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindOnce([](content::BrowserContext* context) {
          return std::unique_ptr<KeyedService>(
              std::make_unique<NiceMock<MockContextualTasksService>>());
        }));

    ContextualTasksUiServiceFactory::GetInstance()->SetTestingFactory(
        profile(), base::BindOnce([](content::BrowserContext* context) {
          Profile* profile = Profile::FromBrowserContext(context);
          return std::unique_ptr<KeyedService>(
              std::make_unique<
                  NiceMock<MockContextualTasksUiServiceForThreadLink>>(
                  profile,
                  ContextualTasksServiceFactory::GetForProfile(profile),
                  /*identity_manager=*/nullptr,
                  /*aim_eligibility_service=*/nullptr,
                  /*eligibility_manager=*/nullptr,
                  /*cookie_synchronizer=*/nullptr));
        }));

    mock_panel_controller_ =
        std::make_unique<NiceMock<MockContextualTasksPanelController>>();

    web_ui_.set_web_contents(web_contents());

    contextual_tasks_ui_ =
        std::make_unique<NiceMock<TestContextualTasksUI>>(&web_ui_);

    // Bind the mock page to the controller.
    contextual_tasks_ui_->GetPageRemote().Bind(page_.BindAndGetRemote());

    mock_contextual_tasks_service_ = static_cast<MockContextualTasksService*>(
        ContextualTasksServiceFactory::GetForProfile(profile()));
    mock_contextual_tasks_ui_service_ =
        static_cast<MockContextualTasksUiServiceForThreadLink*>(
            ContextualTasksUiServiceFactory::GetForBrowserContext(profile()));


    page_handler_ = std::make_unique<ContextualTasksPageHandler>(
        mojo::PendingReceiver<mojom::PageHandler>(), contextual_tasks_ui_.get(),
        mock_contextual_tasks_ui_service_, mock_contextual_tasks_service_,
        mock_panel_controller_.get());
    page_handler_->set_skip_feedback_ui_for_testing(true);
  }

  void TearDown() override {
    page_handler_.reset();
    contextual_tasks_ui_.reset();
    webui::SetBrowserWindowInterface(web_contents(), nullptr);
    mock_panel_controller_.reset();
    mock_contextual_tasks_service_ = nullptr;
    mock_contextual_tasks_ui_service_ = nullptr;
#if !BUILDFLAG(IS_ANDROID)
    actions::ActionIdMap::ResetMapsForTesting();
#endif
    ChromeRenderViewHostTestHarness::TearDown();
    profile_manager_.reset();
  }

 protected:
  content::TestWebUI web_ui_;
  ui::UnownedUserDataHost unowned_user_data_host_;
  std::unique_ptr<MockContextualTasksPanelController> mock_panel_controller_;
  std::unique_ptr<NiceMock<TestContextualTasksUI>> contextual_tasks_ui_;
  std::unique_ptr<ContextualTasksPageHandler> page_handler_;
  raw_ptr<MockContextualTasksService> mock_contextual_tasks_service_;
  raw_ptr<MockContextualTasksUiServiceForThreadLink>
      mock_contextual_tasks_ui_service_;
  NiceMock<MockContextualTasksPage> page_;
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<TestingProfileManager> profile_manager_;
  variations::test::ScopedVariationsIdsProvider scoped_variations_ids_provider_{
      variations::VariationsIdsProvider::Mode::kUseSignedInState};
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

TEST_F(ContextualTasksPageHandlerTest, IsEmbeddedPageErrorDocument_True) {
  std::unique_ptr<content::WebContents> inner_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  // Navigate and fail to create an error document.
  content::NavigationSimulator::NavigateAndFailFromBrowser(
      inner_web_contents.get(), GURL("http://example.com"),
      net::ERR_CONNECTION_RESET);

  EXPECT_CALL(*contextual_tasks_ui_, GetInnerWebContents())
      .WillOnce(Return(inner_web_contents.get()));

  base::RunLoop run_loop;
  page_handler_->IsEmbeddedPageErrorDocument(
      base::BindLambdaForTesting([&](bool is_error_document) {
        EXPECT_TRUE(is_error_document);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ContextualTasksPageHandlerTest, IsEmbeddedPageErrorDocument_False) {
  std::unique_ptr<content::WebContents> inner_web_contents =
      content::WebContentsTester::CreateTestWebContents(profile(), nullptr);

  // Normal successful navigation.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(
      inner_web_contents.get(), GURL("http://example.com"));

  EXPECT_CALL(*contextual_tasks_ui_, GetInnerWebContents())
      .WillOnce(Return(inner_web_contents.get()));

  base::RunLoop run_loop;
  page_handler_->IsEmbeddedPageErrorDocument(
      base::BindLambdaForTesting([&](bool is_error_document) {
        EXPECT_FALSE(is_error_document);
        run_loop.Quit();
      }));
  run_loop.Run();
}

TEST_F(ContextualTasksPageHandlerTest,
       IsEmbeddedPageErrorDocument_NoInnerContents) {
  EXPECT_CALL(*contextual_tasks_ui_, GetInnerWebContents())
      .WillOnce(Return(nullptr));

  base::RunLoop run_loop;
  page_handler_->IsEmbeddedPageErrorDocument(
      base::BindLambdaForTesting([&](bool is_error_document) {
        EXPECT_FALSE(is_error_document);
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

  contextual_search::MockContextualSearchSessionHandle mock_session;
  EXPECT_CALL(*contextual_tasks_ui_, GetOrCreateContextualSessionHandle())
      .WillOnce(Return(&mock_session));

  base::RunLoop run_loop;
  page_handler_->GetUrlForTask(task_id,
                               base::BindLambdaForTesting([&](const GURL& url) {
                                 EXPECT_EQ(url, expected_url);
                                 EXPECT_EQ(
                                     mock_session.previous_turns().back().query,
                                     "test");
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

TEST_F(ContextualTasksPageHandlerTest, GetUrlForTask_CopiesWebUiParams) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  GURL thread_url("https://google.com/search?mtid=123");
  GURL webui_url("chrome://contextual-tasks/?chrome_task_id=" +
                 task_id.AsLowercaseString() + "&p1=1&p2=2");

  EXPECT_CALL(*contextual_tasks_ui_, GetWebUiUrl())
      .WillRepeatedly(Return(webui_url));
  EXPECT_CALL(*mock_contextual_tasks_ui_service_, GetInitialUrlForTask(task_id))
      .WillRepeatedly(Return(std::nullopt));
  EXPECT_CALL(*mock_contextual_tasks_ui_service_,
              GetThreadUrlFromTaskId(task_id, _))
      .WillOnce(
          [&](const base::Uuid&, base::OnceCallback<void(GURL)> callback) {
            std::move(callback).Run(thread_url);
          });

  base::RunLoop run_loop;
  page_handler_->GetUrlForTask(
      task_id, base::BindLambdaForTesting([&](const GURL& url) {
        std::string value;
        EXPECT_TRUE(net::GetValueForKeyInQuery(url, "p1", &value));
        EXPECT_EQ(value, "1");
        EXPECT_TRUE(net::GetValueForKeyInQuery(url, "p2", &value));
        EXPECT_EQ(value, "2");
        EXPECT_FALSE(net::GetValueForKeyInQuery(url, "chrome_task_id", &value));
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
  EXPECT_EQ(composebox_position->max_width, static_cast<uint32_t>(INT32_MAX));
  EXPECT_EQ(composebox_position->max_height, static_cast<uint32_t>(INT32_MAX));
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
  EXPECT_EQ(composebox_position->max_width, 0u);
  EXPECT_EQ(composebox_position->max_height, 0u);
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
                                 testing::Optional(0u)),
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
  EXPECT_CALL(*mock_panel_controller_, Close()).Times(1);
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

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextualTasksPageHandlerTest, PinSidePanel) {
  auto* model = PinnedToolbarActionsModel::Get(profile());
  ASSERT_TRUE(model);

  // Initial state should be unpinned.
  EXPECT_FALSE(model->Contains(kActionSidePanelShowContextualTasks));

  // We expect the page to be notified when the action is pinned.
  EXPECT_CALL(page_, OnSidePanelPinStateChanged(true)).Times(1);

  // Pin the side panel.
  page_handler_->PinSidePanel();
  EXPECT_TRUE(model->Contains(kActionSidePanelShowContextualTasks));

  // Now unpin.
  EXPECT_CALL(page_, OnSidePanelPinStateChanged(false))
      .Times(testing::AtLeast(1));
  page_handler_->UnpinSidePanel();
  EXPECT_FALSE(model->Contains(kActionSidePanelShowContextualTasks));
}

TEST_F(ContextualTasksPageHandlerTest, PinSidePanel_FeatureDisabled) {
  feature_list_.Reset();
  feature_list_.InitAndDisableFeature(
      contextual_tasks::kEnableContextualTasksPinButtonInToolbar);

  auto* model = PinnedToolbarActionsModel::Get(profile());
  ASSERT_TRUE(model);

  // Initial state should be unpinned.
  EXPECT_FALSE(model->Contains(kActionSidePanelShowContextualTasks));

  // Pin the side panel (should be a no-op when feature is disabled).
  page_handler_->PinSidePanel();

  // Should still be false.
  EXPECT_FALSE(model->Contains(kActionSidePanelShowContextualTasks));
}
#endif

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

TEST_F(ContextualTasksPageHandlerTest,
       OnWebviewMessage_OpenLinkInSidePanelMode) {
  lens::AimToClientMessage message;
  auto* open_link = message.mutable_open_link_in_side_panel_mode();
  open_link->set_url("https://example.com");

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);

  EXPECT_CALL(*mock_contextual_tasks_ui_service_,
              OnThreadLinkClicked(GURL("https://example.com"), base::Uuid(),
                                  testing::Eq(nullptr), testing::Eq(nullptr),
                                  testing::_))
      .Times(1);

  page_handler_->OnWebviewMessage(serialized);
}

// Link click events where the URL is not HTTP or HTTPS should not trigger the
// thread link click event.
TEST_F(ContextualTasksPageHandlerTest,
       OnWebviewMessage_NotifyLinkClicked_InvalidScheme) {
  lens::AimToClientMessage message;
  auto* open_link = message.mutable_open_link_in_side_panel_mode();
  open_link->set_url("chrome://settings");

  size_t size = message.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  message.SerializeToArray(serialized.data(), size);

  EXPECT_CALL(*mock_contextual_tasks_ui_service_,
              OnThreadLinkClicked(_, _, _, _, _))
      .Times(0);

  page_handler_->OnWebviewMessage(serialized);
}

TEST_F(ContextualTasksPageHandlerTest, OpenMyActivityUi) {
  // Navigation smoke test. We provide a null browser to safely exit early
  // and avoid crashes in Navigate() which requires a full TabStripModel.
  EXPECT_CALL(*contextual_tasks_ui_, GetBrowser()).WillOnce(Return(nullptr));
  page_handler_->OpenMyActivityUi();
}



TEST_F(ContextualTasksPageHandlerTest, OpenOnboardingHelpUi) {
  // Navigation smoke test.
  EXPECT_CALL(*contextual_tasks_ui_, GetBrowser()).WillOnce(Return(nullptr));
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

#if !BUILDFLAG(IS_ANDROID)
TEST_F(ContextualTasksPageHandlerTest, OnReceivedPinStateChanged) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kEnableContextualTasksPinButtonInToolbar);

  // Ignore any unpinned syncs fired initially by constructor setup.
  EXPECT_CALL(page_, OnSidePanelPinStateChanged(false))
      .Times(testing::AnyNumber());

  // Recreate page_handler_ to pick up the feature flag.
  page_handler_ = std::make_unique<ContextualTasksPageHandler>(
      mojo::PendingReceiver<mojom::PageHandler>(), contextual_tasks_ui_.get(),
      mock_contextual_tasks_ui_service_, mock_contextual_tasks_service_,
      mock_panel_controller_.get());
  page_handler_->set_skip_feedback_ui_for_testing(true);

  base::RunLoop run_loop;
  EXPECT_CALL(page_, OnSidePanelPinStateChanged(true))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  auto* model = PinnedToolbarActionsModel::Get(profile());
  ASSERT_TRUE(model);
  model->UpdatePinnedState(kActionSidePanelShowContextualTasks, true);

  run_loop.Run();
}
#endif  // !BUILDFLAG(IS_ANDROID)

TEST_F(ContextualTasksPageHandlerTest,
       OnReceivedInjectInput_OverridesExisting) {
  // Setup mocks.
  NiceMock<contextual_search::MockContextualSearchSessionHandle>
      mock_session_handle;
  NiceMock<contextual_search::MockContextualSearchContextController>
      mock_controller;

  ON_CALL(mock_session_handle, GetController())
      .WillByDefault(Return(&mock_controller));
  ON_CALL(*contextual_tasks_ui_, GetOrCreateContextualSessionHandle())
      .WillByDefault(Return(&mock_session_handle));

  // Register pre-existing active token.
  base::UnguessableToken old_token = base::UnguessableToken::Create();
  mock_session_handle.GetUploadedContextTokensForTesting().push_back(old_token);

  // Mock corresponding file info mapping to "target_id".
  contextual_search::FileInfo old_file_info;
  old_file_info.input_data = std::make_unique<lens::ContextualInputData>();
  old_file_info.input_data->modality_chip_props = lens::ModalityChipProps();
  old_file_info.input_data->modality_chip_props->set_id("target_id");

  ON_CALL(mock_controller, GetFileInfo(old_token))
      .WillByDefault(Return(&old_file_info));

  // Action prep: Pack the inject request into full AimToClientMessage
  // container.
  lens::AimToClientMessage container;
  auto* input_proto = container.mutable_inject_input();
  input_proto->mutable_modality()->set_id("target_id");
  input_proto->mutable_modality()->set_title("New Title");

  size_t size = container.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  container.SerializeToArray(serialized.data(), size);

  // 1. Verification: Should synchronously drop the OLD token.
  EXPECT_CALL(mock_controller, DeleteFile(old_token)).WillOnce(Return(true));
  EXPECT_CALL(page_, RemoveInjectedInput(old_token)).Times(1);

  // 2. Verification: Should instantiate unique NEW token.
  base::UnguessableToken new_token = base::UnguessableToken::Create();
  EXPECT_CALL(mock_session_handle, CreateContextToken())
      .WillOnce(Return(new_token));

  // 3. Verification: Should successfully pipe NEW injection command to UI.
  base::RunLoop run_loop;
  EXPECT_CALL(page_, InjectInput(_))
      .WillOnce([&run_loop, new_token](mojom::InjectedInputPtr mojo_input) {
        EXPECT_EQ(mojo_input->file_token, new_token);
        run_loop.Quit();
      });

  page_handler_->OnWebviewMessage(serialized);
  run_loop.Run();
}

TEST_F(ContextualTasksPageHandlerTest,
       OnReceivedRemoveInjectedInput_SynchronousDelete) {
  NiceMock<contextual_search::MockContextualSearchSessionHandle>
      mock_session_handle;
  NiceMock<contextual_search::MockContextualSearchContextController>
      mock_controller;

  ON_CALL(mock_session_handle, GetController())
      .WillByDefault(Return(&mock_controller));
  ON_CALL(*contextual_tasks_ui_, GetOrCreateContextualSessionHandle())
      .WillByDefault(Return(&mock_session_handle));

  base::UnguessableToken target_token = base::UnguessableToken::Create();
  mock_session_handle.GetUploadedContextTokensForTesting().push_back(
      target_token);

  contextual_search::FileInfo file_info;
  file_info.input_data = std::make_unique<lens::ContextualInputData>();
  file_info.input_data->modality_chip_props = lens::ModalityChipProps();
  file_info.input_data->modality_chip_props->set_id("removable_id");

  ON_CALL(mock_controller, GetFileInfo(target_token))
      .WillByDefault(Return(&file_info));

  // Verify synchronous remove flow.
  EXPECT_CALL(mock_controller, DeleteFile(target_token)).WillOnce(Return(true));
  base::RunLoop run_loop;
  EXPECT_CALL(page_, RemoveInjectedInput(target_token))
      .WillOnce([&run_loop](const base::UnguessableToken& token) {
        run_loop.Quit();
      });

  // Action prep: Pack and route via the public interface.
  lens::AimToClientMessage container;
  container.mutable_remove_injected_input()->set_id("removable_id");

  size_t size = container.ByteSizeLong();
  std::vector<uint8_t> serialized(size);
  container.SerializeToArray(serialized.data(), size);

  page_handler_->OnWebviewMessage(serialized);
  run_loop.Run();
}

TEST_F(ContextualTasksPageHandlerTest, OnContextMenuOpened) {
  page_handler_->OnContextMenuOpened();
}

}  // namespace contextual_tasks
