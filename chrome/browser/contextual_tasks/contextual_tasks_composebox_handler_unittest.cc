// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_query_flow_router.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/fake_variations_client.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "third_party/omnibox_proto/chrome_aim_entry_point.pb.h"
#include "url/gurl.h"
#include "url/url_constants.h"

class BrowserWindowInterface;
class TemplateURLService;

class LocalContextualSearchboxHandlerTestHarness
    : public BrowserWithTestWindowTest {
 public:
  LocalContextualSearchboxHandlerTestHarness()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_loader_factory_)),
        get_variations_ids_provider_(
            variations::VariationsIdsProvider::Mode::kUseSignedInState) {}
  ~LocalContextualSearchboxHandlerTestHarness() override = default;

  void TearDown() override {
    web_contents_ = nullptr;
    BrowserWithTestWindowTest::TearDown();
  }

  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    AddTab(browser(), GURL(url::kAboutBlankURL));
    web_contents_ =
        TabListInterface::From(browser())->GetActiveTab()->GetContents();
  }

 protected:
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;

  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  contextual_search::FakeVariationsClient fake_variations_client_;
  variations::test::ScopedVariationsIdsProvider get_variations_ids_provider_;

  // Helper methods to access protected members
  content::WebContents* web_contents() { return web_contents_; }
  TestingProfile* profile() { return GetProfile(); }
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return test_shared_loader_factory_;
  }
  TemplateURLService* template_url_service() {
    return template_url_service_.get();
  }
  contextual_search::FakeVariationsClient* fake_variations_client() {
    return &fake_variations_client_;
  }
};

class MockContextualTasksUI : public ContextualTasksUI {
 public:
  explicit MockContextualTasksUI(content::WebUI* web_ui)
      : ContextualTasksUI(web_ui) {}
  MOCK_METHOD(void,
              PostMessageToWebview,
              (const lens::ClientToAimMessage& message),
              (override));
  MOCK_METHOD(content::WebContents*, GetWebUIWebContents, (), (override));
  MOCK_METHOD(const std::optional<base::Uuid>&, GetTaskId, (), (override));
  MOCK_METHOD(BrowserWindowInterface*, GetBrowser, (), (override));
  MOCK_METHOD(bool, IsLensOverlayShowing, (), (const, override));
};

class TestContextualTasksComposeboxHandler
    : public ContextualTasksComposeboxHandler {
 public:
  using ContextualTasksComposeboxHandler::ContextualTasksComposeboxHandler;

  MOCK_METHOD(void,
              UploadTabContextWithData,
              (int32_t tab_id,
               std::optional<int64_t> context_id,
               std::unique_ptr<lens::ContextualInputData> data,
               RecontextualizeTabCallback callback),
              (override));
  MOCK_METHOD(std::optional<base::UnguessableToken>,
              GetLensOverlayToken,
              (),
              (override));

 protected:
  contextual_tasks::ContextualTasksService* GetContextualTasksService()
      override {
    return mock_contextual_tasks_service_;
  }

 public:
  void SetMockContextualTasksService(
      contextual_tasks::ContextualTasksService* contextual_tasks_service) {
    mock_contextual_tasks_service_ = contextual_tasks_service;
  }

  contextual_search::InputStateModel* GetInputStateModelForTesting() {
    return input_state_model_.get();
  }

 private:
  raw_ptr<contextual_tasks::ContextualTasksService>
      mock_contextual_tasks_service_ = nullptr;
};

class MockLensQueryFlowRouter : public lens::LensQueryFlowRouter {
 public:
  explicit MockLensQueryFlowRouter(LensSearchController* controller)
      : lens::LensQueryFlowRouter(controller) {}
  MOCK_METHOD(std::optional<base::UnguessableToken>,
              overlay_tab_context_file_token,
              (),
              (const, override));
};

class MockLensSearchController : public LensSearchController {
 public:
  explicit MockLensSearchController(tabs::TabInterface* tab)
      : LensSearchController(tab) {
    mock_router_ =
        std::make_unique<testing::NiceMock<MockLensQueryFlowRouter>>(this);
  }
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

  lens::LensQueryFlowRouter* query_router() override {
    return mock_router_.get();
  }

  MockLensQueryFlowRouter* mock_router() { return mock_router_.get(); }

 private:
  std::unique_ptr<MockLensQueryFlowRouter> mock_router_;
};

class ContextualTasksComposeboxHandlerTest
    : public LocalContextualSearchboxHandlerTestHarness {
 public:
  ContextualTasksComposeboxHandlerTest() = default;
  ~ContextualTasksComposeboxHandlerTest() override = default;

  void SetUp() override {
    feature_list_.InitAndEnableFeature(contextual_tasks::kContextualTasks);

    // Install override before AddTab is called in base SetUp.
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

    LocalContextualSearchboxHandlerTestHarness::SetUp();
    web_ui_.set_web_contents(web_contents());
    webui::SetTabInterface(web_contents(), nullptr);
    webui::SetBrowserWindowInterface(web_contents(), browser());

    auto mock_controller = std::make_unique<testing::NiceMock<
        contextual_search::MockContextualSearchContextController>>();
    mock_controller_ = mock_controller.get();
    service_ = std::make_unique<contextual_search::ContextualSearchService>(
        /*identity_manager=*/nullptr, url_loader_factory(),
        template_url_service(), fake_variations_client(),
        version_info::Channel::UNKNOWN, "en-US");
    auto contextual_session_handle = service_->CreateSessionForTesting(
        std::move(mock_controller),
        std::make_unique<contextual_search::ContextualSearchMetricsRecorder>(
            contextual_search::ContextualSearchSource::kContextualTasks));
    // Check the search content sharing settings to notify the session handle
    // that the client is properly checking the pref value.
    contextual_session_handle->CheckSearchContentSharingSettings(
        profile()->GetPrefs());
    session_handle_ =
        service_->GetSession(contextual_session_handle->session_id(),
                             /*invocation_source=*/std::nullopt);
    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents())
        ->SetTaskSession(std::nullopt, std::move(contextual_session_handle),
                         /*input_state_model=*/nullptr);

    mock_ui_ =
        std::make_unique<testing::NiceMock<MockContextualTasksUI>>(&web_ui_);
    ON_CALL(*mock_ui_, GetWebUIWebContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(*mock_ui_, GetTaskId())
        .WillByDefault(testing::ReturnRefOfCopy(std::optional<base::Uuid>()));
    ON_CALL(*mock_ui_, GetBrowser()).WillByDefault(testing::Return(browser()));

    // Create mock controller directly.
    mock_contextual_tasks_service_owner_ = std::make_unique<
        testing::NiceMock<contextual_tasks::MockContextualTasksService>>();
    mock_contextual_tasks_service_ptr_ =
        mock_contextual_tasks_service_owner_.get();

    mojo::PendingRemote<composebox::mojom::Page> page_remote;
    page_receiver_ = page_remote.InitWithNewPipeAndPassReceiver();

    handler_ = std::make_unique<TestContextualTasksComposeboxHandler>(
        mock_ui_.get(), profile(), web_contents(),
        mojo::PendingReceiver<composebox::mojom::PageHandler>(),
        std::move(page_remote),
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        base::BindRepeating(
            &ContextualTasksUI::GetOrCreateContextualSessionHandle,
            base::Unretained(mock_ui_.get())),
        base::BindRepeating(&ContextualTasksUI::GetInputStateModel,
                            base::Unretained(mock_ui_.get())));
    handler_->SetMockContextualTasksService(mock_contextual_tasks_service_ptr_);

    auto searchbox_page_remote =
        searchbox_page_receiver_.BindNewPipeAndPassRemote();
    handler_->SetPage(std::move(searchbox_page_remote));

    // Setup MockTabContextualizationController
    tabs::TabInterface* active_tab =
        TabListInterface::From(browser())->GetActiveTab();
    // Clear existing controller to avoid UserData collision
    active_tab->GetTabFeatures()->SetTabContextualizationControllerForTesting(
        nullptr);

    auto mock_tab_controller =
        std::make_unique<MockTabContextualizationController>(active_tab);
    mock_tab_controller_ = mock_tab_controller.get();
    active_tab->GetTabFeatures()->SetTabContextualizationControllerForTesting(
        std::move(mock_tab_controller));

    ASSERT_TRUE(mock_lens_controller_);
  }

  std::unique_ptr<contextual_tasks::MockContextualTasksService>
      mock_contextual_tasks_service_owner_;

  void TearDown() override {
    // Manually verify and clear expectations to avoid issues during teardown
    // when the tab is closed and CloseLensSync is called again with kTabClosed.
    testing::Mock::VerifyAndClearExpectations(mock_lens_controller_);

    // Reset handler first to destroy the omnibox client which observes the
    // lens controller.
    handler_.reset();
    mock_controller_ = nullptr;
    mock_contextual_tasks_service_ptr_ = nullptr;
    mock_tab_controller_ = nullptr;
    mock_lens_controller_ = nullptr;
    session_handle_.reset();
    service_.reset();
    mock_ui_.reset();
    LocalContextualSearchboxHandlerTestHarness::TearDown();
  }

 protected:
  content::TestWebUI web_ui_;
  std::unique_ptr<testing::NiceMock<MockContextualTasksUI>> mock_ui_;
  std::unique_ptr<TestContextualTasksComposeboxHandler> handler_;

  // For session management.
  std::unique_ptr<contextual_search::ContextualSearchService> service_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  raw_ptr<contextual_search::MockContextualSearchContextController>
      mock_controller_;
  raw_ptr<contextual_tasks::MockContextualTasksService>
      mock_contextual_tasks_service_ptr_ = nullptr;

  raw_ptr<MockTabContextualizationController> mock_tab_controller_ = nullptr;
  raw_ptr<MockLensSearchController> mock_lens_controller_ = nullptr;

  testing::NiceMock<MockSearchboxPage> mock_searchbox_page_;
  mojo::Receiver<searchbox::mojom::Page> searchbox_page_receiver_{
      &mock_searchbox_page_};

 private:
  base::test::ScopedFeatureList feature_list_;
  ui::UserDataFactory::ScopedOverride lens_controller_override_;
  mojo::PendingReceiver<composebox::mojom::Page> page_receiver_;
};

TEST_F(ContextualTasksComposeboxHandlerTest, SubmitQuery) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL in SubmitQuery!";
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->SubmitQuery("test query", 0, false, false, false, false);
}

TEST_F(ContextualTasksComposeboxHandlerTest, CreateAndSendQueryMessage) {
  std::string kQuery = "direct query";
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::optional<base::Uuid>()));
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&kQuery](std::unique_ptr<
                          contextual_search::ContextualSearchContextController::
                              CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->query_text, kQuery);
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_WithOverlayToken) {
  std::string kQuery = "direct query";
  base::UnguessableToken overlay_token = base::UnguessableToken::Create();

  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::optional<base::Uuid>()));
  EXPECT_CALL(*handler_, GetLensOverlayToken())
      .WillOnce(testing::Return(overlay_token));

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&kQuery, &overlay_token](
                    std::unique_ptr<
                        contextual_search::ContextualSearchContextController::
                            CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->query_text, kQuery);
        EXPECT_THAT(info->file_tokens, testing::Contains(overlay_token));
        EXPECT_TRUE(info->force_include_latest_interaction_request_data);
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_NoOverlayToken) {
  std::string kQuery = "direct query";

  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::optional<base::Uuid>()));
  EXPECT_CALL(*handler_, GetLensOverlayToken())
      .WillOnce(testing::Return(std::nullopt));

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&kQuery](std::unique_ptr<
                          contextual_search::ContextualSearchContextController::
                              CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->query_text, kQuery);
        EXPECT_FALSE(info->force_include_latest_interaction_request_data);
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_RecontextualizeExpiredTab) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "recontextualize query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context with expired tab.
  contextual_tasks::ContextualTask task(task_id);
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  GURL kUrl("about:blank");
  // The default title for about:blank is "about:blank".
  std::string kTitle = "about:blank";

  contextual_tasks::UrlResource resource(
      kUrl, contextual_tasks::ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_contextual_tasks_service_ptr_,
      GetContextForTask(
          task_id,
          testing::Contains(contextual_tasks::ContextualTaskContextSource::
                                kPendingContextDecorator),
          testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with expired status.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status = contextual_search::FileUploadStatus::kUploadExpired;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);
  file_info_list.push_back(&file_info);

  EXPECT_CALL(*mock_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  // Expect GetPageContext call.
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillOnce([session_id](
                    MockTabContextualizationController::GetPageContextCallback
                        callback) {
        // Return some dummy content.
        auto data = std::make_unique<lens::ContextualInputData>();
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect UploadTabContextWithData call.
  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_,
                                                  std::optional<int64_t>(12345),
                                                  testing::_, testing::_))
      .WillOnce(
          [](int32_t tab_id, std::optional<int64_t> context_id,
             std::unique_ptr<lens::ContextualInputData> data,
             ContextualSearchboxHandler::RecontextualizeTabCallback callback) {
            std::move(callback).Run(true);
          });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_RecontextualizeContentChanged) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "recontextualize query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context with uploaded tab (not expired).
  contextual_tasks::ContextualTask task(task_id);
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  contextual_tasks::UrlResource resource(
      kUrl, contextual_tasks::ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_contextual_tasks_service_ptr_,
      GetContextForTask(
          task_id,
          testing::Contains(contextual_tasks::ContextualTaskContextSource::
                                kPendingContextDecorator),
          testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup context with uploaded status and some previous content.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::FileUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*mock_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  // Expect GetPageContext call with NEW content.
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillOnce([session_id](
                    MockTabContextualizationController::GetPageContextCallback
                        callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        std::string new_content = "new content";
        auto new_content_span = base::as_bytes(base::span(new_content));
        std::vector<uint8_t> new_bytes(new_content_span.begin(),
                                       new_content_span.end());
        lens::ContextualInput new_input(std::move(new_bytes),
                                        lens::MimeType::kPlainText);
        data->context_input.emplace().push_back(std::move(new_input));
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect UploadTabContextWithData call because content changed.
  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_,
                                                  std::optional<int64_t>(12345),
                                                  testing::_, testing::_))
      .WillOnce(
          [](int32_t tab_id, std::optional<int64_t> context_id,
             std::unique_ptr<lens::ContextualInputData> data,
             ContextualSearchboxHandler::RecontextualizeTabCallback callback) {
            std::move(callback).Run(true);
          });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_NoRecontextualizationIfUnchanged) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "valid tab query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context with uploaded tab
  contextual_tasks::ContextualTask task(task_id);
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  contextual_tasks::UrlResource resource(
      GURL("about:blank"), contextual_tasks::ResourceType::kWebpage);
  resource.title = "about:blank";
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_contextual_tasks_service_ptr_,
      GetContextForTask(
          task_id,
          testing::Contains(contextual_tasks::ContextualTaskContextSource::
                                kPendingContextDecorator),
          testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with uploaded status and SAME content.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::FileUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  // Populate input_data for comparison
  auto input_data = std::make_unique<lens::ContextualInputData>();
  std::string content = "same content";
  auto content_span = base::as_bytes(base::span(content));
  std::vector<uint8_t> bytes(content_span.begin(), content_span.end());
  lens::ContextualInput input(std::move(bytes), lens::MimeType::kPlainText);
  input_data->context_input.emplace().push_back(std::move(input));
  input_data->primary_content_type = lens::MimeType::kPlainText;
  file_info.input_data = std::move(input_data);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*mock_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  // Expect GetPageContext call with SAME content.
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillOnce([session_id](
                    MockTabContextualizationController::GetPageContextCallback
                        callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        // Reconstruct same input.
        std::string content = "same content";
        auto content_span = base::as_bytes(base::span(content));
        std::vector<uint8_t> bytes(content_span.begin(), content_span.end());
        lens::ContextualInput new_input(std::move(bytes),
                                        lens::MimeType::kPlainText);
        data->context_input.emplace().push_back(std::move(new_input));
        data->primary_content_type = lens::MimeType::kPlainText;
        // Set session ID to match
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        std::move(callback).Run(std::move(data));
      });

  // Expect UploadTabContextWithData call to NOT be called.
  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_, testing::_,
                                                  testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_ActiveTabNotInContext) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "query with unrelated active tab";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context with NO tabs (or just not the active one).
  contextual_tasks::ContextualTask task(task_id);
  // Add a resource that is NOT the active tab.
  contextual_tasks::UrlResource resource(
      GURL("http://example.com"), contextual_tasks::ResourceType::kWebpage);
  resource.tab_id = SessionID::NewUnique();  // Random ID
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_contextual_tasks_service_ptr_,
      GetContextForTask(
          task_id,
          testing::Contains(contextual_tasks::ContextualTaskContextSource::
                                kPendingContextDecorator),
          testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Expect UploadTabContextWithData to NOT be called.
  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_, testing::_,
                                                  testing::_, testing::_))
      .Times(0);

  // Expect CreateClientToAimRequest IS called (query submission continues).
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_ActiveTabUrlMismatch) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "query with url mismatch";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context with matching SessionID but mismatching URL.
  // Active tab is at about:blank. Resource is at http://example.com.
  contextual_tasks::ContextualTask task(task_id);
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  GURL kUrl("http://example.com");
  std::u16string kTitle = u"Example Title";

  contextual_tasks::UrlResource resource(
      kUrl, contextual_tasks::ResourceType::kWebpage);
  resource.title = base::UTF16ToUTF8(kTitle);
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_contextual_tasks_service_ptr_,
      GetContextForTask(
          task_id,
          testing::Contains(contextual_tasks::ContextualTaskContextSource::
                                kPendingContextDecorator),
          testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Expect UploadTabContextWithData to NOT be called.
  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_, testing::_,
                                                  testing::_, testing::_))
      .Times(0);

  // Expect CreateClientToAimRequest IS called.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_RecontextualizeScreenshotChanged_SkBitmap) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "recontextualize query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context with uploaded tab.
  contextual_tasks::ContextualTask task(task_id);
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  contextual_tasks::UrlResource resource(
      kUrl, contextual_tasks::ResourceType::kWebpage);
  resource.title = kTitle;
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_contextual_tasks_service_ptr_,
      GetContextForTask(
          task_id,
          testing::Contains(contextual_tasks::ContextualTaskContextSource::
                                kPendingContextDecorator),
          testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with uploaded status and OLD bitmap.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::FileUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  auto input_data = std::make_unique<lens::ContextualInputData>();
  SkBitmap old_bitmap;
  old_bitmap.allocN32Pixels(10, 10);
  old_bitmap.eraseColor(SK_ColorRED);
  input_data->viewport_screenshot = old_bitmap;
  file_info.input_data = std::move(input_data);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*mock_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  // Expect GetPageContext call with NEW bitmap (different color).
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillOnce([session_id](
                    MockTabContextualizationController::GetPageContextCallback
                        callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        SkBitmap new_bitmap;
        new_bitmap.allocN32Pixels(10, 10);
        new_bitmap.eraseColor(SK_ColorBLUE);
        data->viewport_screenshot = new_bitmap;
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect UploadTabContextWithData call because bitmap changed.
  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_,
                                                  std::optional<int64_t>(12345),
                                                  testing::_, testing::_))
      .WillOnce(
          [](int32_t tab_id, std::optional<int64_t> context_id,
             std::unique_ptr<lens::ContextualInputData> data,
             ContextualSearchboxHandler::RecontextualizeTabCallback callback) {
            std::move(callback).Run(true);
          });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
  base::RunLoop().RunUntilIdle();
}

TEST_F(
    ContextualTasksComposeboxHandlerTest,
    CreateAndSendQueryMessage_NoRecontextualizationIfScreenshotUnchanged_SkBitmap) {
  // Test case for no recontextualization.
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "valid tab query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context with uploaded tab
  contextual_tasks::ContextualTask task(task_id);
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  contextual_tasks::UrlResource resource(
      GURL("about:blank"), contextual_tasks::ResourceType::kWebpage);
  resource.title = "about:blank";
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_contextual_tasks_service_ptr_,
      GetContextForTask(
          task_id,
          testing::Contains(contextual_tasks::ContextualTaskContextSource::
                                kPendingContextDecorator),
          testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with uploaded status and SAME bitmap.
  std::vector<const contextual_search::FileInfo*> file_info_list;
  contextual_search::FileInfo file_info;
  file_info.tab_session_id = session_id;
  file_info.upload_status =
      contextual_search::FileUploadStatus::kUploadSuccessful;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);

  auto input_data = std::make_unique<lens::ContextualInputData>();
  SkBitmap old_bitmap;
  old_bitmap.allocN32Pixels(10, 10);
  old_bitmap.eraseColor(SK_ColorRED);
  input_data->viewport_screenshot = old_bitmap;
  file_info.input_data = std::move(input_data);

  file_info_list.push_back(&file_info);

  EXPECT_CALL(*mock_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  // Expect GetPageContext call with SAME bitmap.
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillOnce([session_id](
                    MockTabContextualizationController::GetPageContextCallback
                        callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        SkBitmap new_bitmap;
        new_bitmap.allocN32Pixels(10, 10);
        new_bitmap.eraseColor(SK_ColorRED);
        data->viewport_screenshot = new_bitmap;
        data->tab_session_id = session_id;
        data->page_url = GURL("about:blank");
        data->page_title = "about:blank";
        data->context_id = 12345;
        std::move(callback).Run(std::move(data));
      });

  // Expect UploadTabContextWithData call to NOT be called.
  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_, testing::_,
                                                  testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualTasksComposeboxHandlerTest, OnAutocompleteAccept) {
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  AutocompleteMatch match;
  handler_->GetOmniboxControllerForTesting()->client()->OnAutocompleteAccept(
      GURL("https://www.google.com/search?q=test query"), nullptr,
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED,
      AutocompleteMatchType::SEARCH_SUGGEST, base::TimeTicks::Now(), false,
      false, u"test query", match, match);
}

TEST_F(ContextualTasksComposeboxHandlerTest, HandleLensButtonClick) {
  EXPECT_CALL(
      *mock_lens_controller_,
      OpenLensOverlay(
          lens::LensOverlayInvocationSource::kContextualTasksComposebox, true));
  handler_->HandleLensButtonClick();
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       OnAutocompleteAccept_ExtractsQuery) {
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([](std::unique_ptr<
                    contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->query_text, "extracted query");
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  AutocompleteMatch match;
  handler_->GetOmniboxControllerForTesting()->client()->OnAutocompleteAccept(
      GURL("https://www.google.com/search?q=extracted%20query"), nullptr,
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED,
      AutocompleteMatchType::SEARCH_SUGGEST, base::TimeTicks::Now(), false,
      false, u"extracted query", match, match);
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       OnAutocompleteAccept_NoQueryParam) {
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([](std::unique_ptr<
                    contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->query_text, "");
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  AutocompleteMatch match;
  handler_->GetOmniboxControllerForTesting()->client()->OnAutocompleteAccept(
      GURL("https://www.google.com/search?other=param"), nullptr,
      WindowOpenDisposition::CURRENT_TAB, ui::PAGE_TRANSITION_TYPED,
      AutocompleteMatchType::SEARCH_SUGGEST, base::TimeTicks::Now(), false,
      false, u"other param", match, match);
}

struct ToolModeTestParam {
  omnibox::ToolMode tool_mode;
};

class ContextualTasksComposeboxHandlerToolModeTest
    : public ContextualTasksComposeboxHandlerTest,
      public ::testing::WithParamInterface<ToolModeTestParam> {};

TEST_P(ContextualTasksComposeboxHandlerToolModeTest, SetsToolModeFlags) {
  const auto& param = GetParam();

  handler_->SetActiveToolMode(param.tool_mode);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&](std::unique_ptr<
                    contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->active_tool, param.tool_mode);
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage("test query");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksComposeboxHandlerToolModeTest,
    ::testing::Values(
        ToolModeTestParam{omnibox::ToolMode::TOOL_MODE_UNSPECIFIED},
        ToolModeTestParam{omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH},
        ToolModeTestParam{omnibox::ToolMode::TOOL_MODE_IMAGE_GEN},
        ToolModeTestParam{omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD}));

TEST_F(ContextualTasksComposeboxHandlerTest, AddTabContext_Delayed) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "delayed tab query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context.
  contextual_tasks::ContextualTask task(task_id);
  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_contextual_tasks_service_ptr_,
      GetContextForTask(
          task_id,
          testing::Contains(contextual_tasks::ContextualTaskContextSource::
                                kPendingContextDecorator),
          testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);

  // 1. Add delayed tab context.
  int32_t tab_id = 100;
  std::optional<base::UnguessableToken> token_opt;
  base::MockCallback<ContextualSearchboxHandler::AddTabContextCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    token_opt = result.value();
  });

  handler_->AddTabContext(tab_id, /*delay_upload=*/true, callback.Get());
  ASSERT_TRUE(token_opt.has_value());
  base::UnguessableToken token = token_opt.value();
  ASSERT_FALSE(token.is_empty());

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 1);

  // 2. Verify tab is added to GetTabsToUpdate (via CreateAndSendQueryMessage).
  // We need to mock the tab handle resolution. Since we can't easily mock
  // TabHandle::Get() for arbitrary IDs in this test harness without more setup,
  // we will use the active tab's ID which IS set up.
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  int32_t active_tab_id = active_tab->GetHandle().raw_value();

  // Reset and try again with active tab ID.
  std::optional<base::UnguessableToken> active_token_opt;
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    active_token_opt = result.value();
  });
  handler_->AddTabContext(active_tab_id, /*delay_upload=*/true, callback.Get());
  ASSERT_TRUE(active_token_opt.has_value());
  base::UnguessableToken active_token = active_token_opt.value();
  ASSERT_FALSE(active_token.is_empty());

  // Expect GetPageContext call for the active tab.
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillOnce([](MockTabContextualizationController::GetPageContextCallback
                       callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  // Expect UploadTabContextWithData call.
  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_, testing::_,
                                                  testing::_, testing::_))
      .WillOnce(
          [](int32_t tab_id, std::optional<int64_t> context_id,
             std::unique_ptr<lens::ContextualInputData> data,
             ContextualSearchboxHandler::RecontextualizeTabCallback callback) {
            std::move(callback).Run(true);
          });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
  base::RunLoop().RunUntilIdle();

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);
}

TEST_F(ContextualTasksComposeboxHandlerTest, DeleteContext_Delayed) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "delete context query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context.
  contextual_tasks::ContextualTask task(task_id);
  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_contextual_tasks_service_ptr_,
      GetContextForTask(
          task_id,
          testing::Contains(contextual_tasks::ContextualTaskContextSource::
                                kPendingContextDecorator),
          testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // 1. Add delayed tab context.
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  int32_t active_tab_id = active_tab->GetHandle().raw_value();
  std::optional<base::UnguessableToken> token_opt;
  base::MockCallback<ContextualSearchboxHandler::AddTabContextCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    token_opt = result.value();
  });

  handler_->AddTabContext(active_tab_id, /*delay_upload=*/true, callback.Get());
  ASSERT_TRUE(token_opt.has_value());
  base::UnguessableToken token = token_opt.value();
  ASSERT_FALSE(token.is_empty());

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 1);

  // 2. Delete the context.
  handler_->DeleteContext(token, /*from_automatic_chip=*/true);

  // No stashed message since we have not submitted a query yet,
  // nor uploaded the delayed tab yet.
  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);

  // 3. Verify UploadTabContextWithData is NOT called when submitting.
  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_, testing::_,
                                                  testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));
  handler_->CreateAndSendQueryMessage(kQuery);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualTasksComposeboxHandlerTest, SubmitQuery_WaitsForUpload) {
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_NE(active_tab, nullptr) << "No active tab found.";

  int32_t tab_handle_id = active_tab->GetHandle().raw_value();
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  contextual_tasks::ContextualTask task(task_id);

  // Set mock taskID for when submit query/upload file.
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Mock getting tab's content by mocking the 2 functions
  // that start tab uploads until barrier closure.
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillRepeatedly([session_id](auto callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->context_id = 123;
        data->tab_session_id = session_id;
        data->is_page_context_eligible = true;
        data->page_url = GURL("https://example.com");
        std::move(callback).Run(std::move(data));
      });

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);
  EXPECT_CALL(*mock_contextual_tasks_service_ptr_,
              GetContextForTask(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [&context](
              const base::Uuid&,
              const std::set<contextual_tasks::ContextualTaskContextSource>&,
              std::unique_ptr<contextual_tasks::ContextDecorationParams>,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Expect client request is formulated.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillRepeatedly(testing::Return(lens::ClientToAimMessage()));

  // Run add tab context's callback via mock so can store token in test.
  base::MockCallback<ContextualTasksComposeboxHandler::AddTabContextCallback>
      callback;
  std::optional<base::UnguessableToken> token_opt;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    token_opt = result.value();
    run_loop.Quit();
  });

  handler_->AddTabContext(tab_handle_id, /*delay_upload=*/false,
                          callback.Get());
  run_loop.Run();

  ASSERT_TRUE(token_opt.has_value()) << "AddTabContext failed.";
  base::UnguessableToken token = *token_opt;

  // Mock file upload status:
  contextual_search::FileInfo uploading_info{};
  uploading_info.upload_status =
      contextual_search::FileUploadStatus::kProcessing;
  uploading_info.mime_type = lens::MimeType::kPdf;
  uploading_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  EXPECT_CALL(*mock_controller_, GetFileInfo(token))
      .WillRepeatedly(testing::Return(&uploading_info));
  // Do not submit request to server yet.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  handler_->SubmitQuery("Summarize the tab", 0, false, false, false, false);
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());

  // Now, once file is successfully uploaded, should send request to server.
  uploading_info.upload_status =
      contextual_search::FileUploadStatus::kUploadSuccessful;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  handler_->OnFileUploadStatusChanged(
      token, lens::MimeType::kPdf,
      contextual_search::FileUploadStatus::kUploadSuccessful, std::nullopt);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       SubmitQuery_ImageReplacedThenOtherTerminalStates) {
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_NE(active_tab, nullptr) << "No active tab found.";

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  contextual_tasks::ContextualTask task(task_id);

  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  auto file_info = searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test.pdf";
  file_info->mime_type = "application/pdf";
  file_info->is_deletable = true;
  std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
  mojo_base::BigBuffer file_bytes(data);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillRepeatedly(testing::Return(lens::ClientToAimMessage()));

  // Only execute part of context upload compared to past tests
  // to verify early return's in upload callback workflow.
  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(*mock_contextual_tasks_service_ptr_,
              GetContextForTask(testing::_, testing::_, testing::_, testing::_))
      .WillRepeatedly(
          [&context](
              const base::Uuid&,
              const std::set<contextual_tasks::ContextualTaskContextSource>&,
              std::unique_ptr<contextual_tasks::ContextDecorationParams>,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Run add file context's callback via mock so can store token in test.
  base::MockCallback<ContextualTasksComposeboxHandler::AddFileContextCallback>
      callback;
  std::optional<base::UnguessableToken> current_token;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    current_token = result.value();
    run_loop.Quit();
  });

  handler_->AddFileContext(std::move(file_info), std::move(file_bytes),
                           callback.Get());
  run_loop.Run();

  ASSERT_TRUE(current_token.has_value()) << "AddFileContext failed.";

  handler_->OnFileUploadStatusChanged(
      *current_token, lens::MimeType::kImage,
      contextual_search::FileUploadStatus::kProcessing, std::nullopt);
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  handler_->OnFileUploadStatusChanged(
      *current_token, lens::MimeType::kImage,
      contextual_search::FileUploadStatus::kUploadReplaced, std::nullopt);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  auto file_info_2 = searchbox::mojom::SelectedFileInfo::New();
  file_info_2->file_name = "test2.pdf";
  file_info_2->mime_type = "application/pdf";
  file_info_2->is_deletable = true;
  std::vector<uint8_t> data_2 = {0xDE, 0xAD, 0xBE, 0xEF};
  mojo_base::BigBuffer file_bytes_2(data_2);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillRepeatedly(testing::Return(lens::ClientToAimMessage()));

  // Run add file context's callback via mock so can store token in test.
  base::MockCallback<ContextualTasksComposeboxHandler::AddFileContextCallback>
      callback_2;
  std::optional<base::UnguessableToken> current_token_2;
  base::RunLoop run_loop_2;
  EXPECT_CALL(callback_2, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    current_token_2 = result.value();
    run_loop_2.Quit();
  });

  handler_->AddFileContext(std::move(file_info_2), std::move(file_bytes_2),
                           callback_2.Get());
  run_loop_2.Run();

  ASSERT_TRUE(current_token_2.has_value()) << "AddFileContext failed.";

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  handler_->OnFileUploadStatusChanged(
      *current_token_2, lens::MimeType::kImage,
      contextual_search::FileUploadStatus::kProcessing, std::nullopt);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  handler_->OnFileUploadStatusChanged(
      *current_token_2, lens::MimeType::kImage,
      contextual_search::FileUploadStatus::kNotUploaded, std::nullopt);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  handler_->OnFileUploadStatusChanged(
      *current_token_2, lens::MimeType::kImage,
      contextual_search::FileUploadStatus::kUploadStarted, std::nullopt);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  handler_->OnFileUploadStatusChanged(
      *current_token_2, lens::MimeType::kImage,
      contextual_search::FileUploadStatus::kProcessingSuggestSignalsReady,
      std::nullopt);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  handler_->SubmitQuery("What is this?", 0, false, false, false, false);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());

  testing::Mock::VerifyAndClearExpectations(mock_ui_.get());
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  handler_->OnFileUploadStatusChanged(
      *current_token_2, lens::MimeType::kImage,
      contextual_search::FileUploadStatus::kUploadExpired, std::nullopt);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       SubmitQuery_ThenDeleteToTriggerFullSubmit) {
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_NE(active_tab, nullptr) << "No active tab found.";

  int32_t tab_handle_id = active_tab->GetHandle().raw_value();
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  contextual_tasks::ContextualTask task(task_id);

  // Set mock taskID for when submit query/upload file.
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Mock getting tab's content by mocking the 2 functions
  // that start tab uploads until barrier closure.
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillRepeatedly([session_id](auto callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->context_id = 123;
        data->tab_session_id = session_id;
        data->is_page_context_eligible = true;
        data->page_url = GURL("https://example.com");
        std::move(callback).Run(std::move(data));
      });

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);
  EXPECT_CALL(*mock_contextual_tasks_service_ptr_,
              GetContextForTask(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [&context](
              const base::Uuid&,
              const std::set<contextual_tasks::ContextualTaskContextSource>&,
              std::unique_ptr<contextual_tasks::ContextDecorationParams>,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Expect client request is formulated.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillRepeatedly(testing::Return(lens::ClientToAimMessage()));

  // Run add tab context's callback via mock so can store token in test.
  base::MockCallback<ContextualTasksComposeboxHandler::AddTabContextCallback>
      callback;
  std::optional<base::UnguessableToken> token_opt;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    token_opt = result.value();
    run_loop.Quit();
  });

  handler_->AddTabContext(tab_handle_id, /*delay_upload=*/false,
                          callback.Get());
  run_loop.Run();

  ASSERT_TRUE(token_opt.has_value()) << "AddTabContext failed.";
  base::UnguessableToken token = *token_opt;

  // Mock file upload status:
  contextual_search::FileInfo uploading_info{};
  uploading_info.upload_status =
      contextual_search::FileUploadStatus::kProcessing;
  uploading_info.mime_type = lens::MimeType::kPdf;
  uploading_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  EXPECT_CALL(*mock_controller_, GetFileInfo(token))
      .WillRepeatedly(testing::Return(&uploading_info));
  // Do not submit request to server yet.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  // Should stash message instead of submit.
  handler_->SubmitQuery("Summarize the tab", 0, false, false, false, false);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 1);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);

  // Deleting last file uploading should trigger full submit.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);

  handler_->DeleteContext(token, /*from_automatic_chip=*/true);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       SubmitQuery_AfterDeleteLastUploadingFile) {
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_NE(active_tab, nullptr) << "No active tab found.";

  int32_t tab_handle_id = active_tab->GetHandle().raw_value();
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  base::Uuid task_id = base::Uuid::GenerateRandomV4();

  contextual_tasks::ContextualTask task(task_id);

  // Set mock taskID for when submit query/upload file.
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Mock getting tab's content by mocking the 2 functions
  // that start tab uploads until barrier closure.
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillRepeatedly([session_id](auto callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->context_id = 123;
        data->tab_session_id = session_id;
        data->is_page_context_eligible = true;
        data->page_url = GURL("https://example.com");
        std::move(callback).Run(std::move(data));
      });

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);
  EXPECT_CALL(*mock_contextual_tasks_service_ptr_,
              GetContextForTask(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [&context](
              const base::Uuid&,
              const std::set<contextual_tasks::ContextualTaskContextSource>&,
              std::unique_ptr<contextual_tasks::ContextDecorationParams>,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Expect client request is formulated.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillRepeatedly(testing::Return(lens::ClientToAimMessage()));

  // Run add tab context's callback via mock so can store token in test.
  base::MockCallback<ContextualTasksComposeboxHandler::AddTabContextCallback>
      callback;
  std::optional<base::UnguessableToken> token_opt;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    token_opt = result.value();
    run_loop.Quit();
  });

  handler_->AddTabContext(tab_handle_id, /*delay_upload=*/false,
                          callback.Get());
  run_loop.Run();

  ASSERT_TRUE(token_opt.has_value()) << "AddTabContext failed.";
  base::UnguessableToken token = *token_opt;

  // Mock file upload status:
  contextual_search::FileInfo uploading_info{};
  uploading_info.upload_status =
      contextual_search::FileUploadStatus::kProcessing;
  uploading_info.mime_type = lens::MimeType::kPdf;
  uploading_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  EXPECT_CALL(*mock_controller_, GetFileInfo(token))
      .WillRepeatedly(testing::Return(&uploading_info));
  // Do not submit request to server yet.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  handler_->SubmitQuery("Summarize the tab", 0, false, false, false, false);
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());

  // Now, once file is deleted, should send request to server.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  handler_->DeleteContext(token, /*from_automatic_chip=*/true);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       SubmitQuery_WaitsForDelayedUpload) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is null.";
  std::string kQuery = "recontextualize query";

  // Setup context with uploaded tab (not expired).
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  GURL kUrl("about:blank");
  std::string kTitle = "about:blank";

  contextual_tasks::UrlResource resource(
      kUrl, contextual_tasks::ResourceType::kUnknown);
  resource.title = kTitle;
  resource.tab_id = session_id;

  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_NE(active_tab, nullptr) << "No active tab found!.";
  int32_t tab_handle_id = active_tab->GetHandle().raw_value();

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  contextual_tasks::ContextualTask task(task_id);
  task.AddUrlResource(resource);

  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  // Execute full context upload compared to past tests
  // to verify full upload callback workflow.
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillRepeatedly([session_id](auto callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->context_id = 123;
        data->tab_session_id = session_id;
        data->is_page_context_eligible = true;
        data->page_url = GURL("https://example.com");
        std::move(callback).Run(std::move(data));
      });
  EXPECT_CALL(
      *mock_contextual_tasks_service_ptr_,
      GetContextForTask(
          task_id,
          testing::Contains(contextual_tasks::ContextualTaskContextSource::
                                kPendingContextDecorator),
          testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });
  EXPECT_CALL(*handler_,
              UploadTabContextWithData(testing::_, testing::Eq(std::nullopt),
                                       testing::_, testing::_))
      .WillOnce(
          [](int32_t tab_id, std::optional<int64_t> context_id,
             std::unique_ptr<lens::ContextualInputData> data,
             ContextualSearchboxHandler::RecontextualizeTabCallback callback) {
            std::move(callback).Run(true);
          });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillRepeatedly(testing::Return(lens::ClientToAimMessage()));

  // Run add tab context's callback via mock so can store token in test.
  base::MockCallback<ContextualTasksComposeboxHandler::AddTabContextCallback>
      callback;
  std::optional<base::UnguessableToken> token_opt;
  base::RunLoop run_loop;

  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    token_opt = result.value();
    run_loop.Quit();
  });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  handler_->AddTabContext(tab_handle_id, /*delay_upload=*/true, callback.Get());
  run_loop.Run();

  ASSERT_TRUE(token_opt.has_value())
      << "AddTabContext failed. URL setup might be wrong.";
  base::UnguessableToken token = *token_opt;
  contextual_search::FileInfo uploading_info{};
  uploading_info.mime_type = lens::MimeType::kPdf;
  uploading_info.upload_status =
      contextual_search::FileUploadStatus::kProcessing;
  uploading_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());

  EXPECT_CALL(*mock_controller_, GetFileInfo(token))
      .WillRepeatedly(testing::Return(&uploading_info));
  // Should submit when SubmitQuery run + delayed tabs finish uploading.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 1);
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  // No pending query yet since have not submitted yet.
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  handler_->SubmitQuery("What is this?", 0, false, false, false, false);

  // Now the delayed tabs should have uploaded.
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);
  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

TEST_F(ContextualTasksComposeboxHandlerTest, SubmitQuery_Immediately) {
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_NE(active_tab, nullptr) << "No active tab found.";

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  contextual_tasks::ContextualTask task(task_id);

  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  auto file_info = searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test.pdf";
  file_info->mime_type = "application/pdf";
  file_info->is_deletable = true;
  std::vector<uint8_t> data = {0xDE, 0xAD, 0xBE, 0xEF};
  mojo_base::BigBuffer file_bytes(data);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillRepeatedly(testing::Return(lens::ClientToAimMessage()));

  // Only execute part of context upload compared to past tests
  // to verify early return's in upload callback workflow.
  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);
  EXPECT_CALL(*mock_contextual_tasks_service_ptr_,
              GetContextForTask(testing::_, testing::_, testing::_, testing::_))
      .WillOnce(
          [&context](
              const base::Uuid&,
              const std::set<contextual_tasks::ContextualTaskContextSource>&,
              std::unique_ptr<contextual_tasks::ContextDecorationParams>,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Run add file context's callback via mock so can store token in test.
  base::MockCallback<ContextualTasksComposeboxHandler::AddFileContextCallback>
      callback;
  std::optional<base::UnguessableToken> current_token;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    current_token = result.value();
    run_loop.Quit();
  });

  handler_->AddFileContext(std::move(file_info), std::move(file_bytes),
                           callback.Get());
  run_loop.Run();

  ASSERT_TRUE(current_token.has_value()) << "AddFileContext failed.";

  // File is not finished uploading.
  contextual_search::FileInfo uploading_info{};
  uploading_info.mime_type = lens::MimeType::kPdf;
  uploading_info.upload_status =
      contextual_search::FileUploadStatus::kProcessing;
  uploading_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  EXPECT_CALL(*mock_controller_, GetFileInfo(*current_token))
      .WillRepeatedly(testing::Return(&uploading_info));

  handler_->SubmitQuery("What is this?", 0, false, false, false, false);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());

  // File is finished uploading.
  uploading_info.upload_status =
      contextual_search::FileUploadStatus::kUploadSuccessful;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  handler_->OnFileUploadStatusChanged(
      *current_token, lens::MimeType::kPdf,
      contextual_search::FileUploadStatus::kUploadSuccessful, std::nullopt);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       SubmitQuery_WaitsForFilesAndDelayedTabs) {
  // Set up tabs and functions that return them.
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  ASSERT_NE(active_tab, nullptr) << "No active tab found.";
  int32_t tab_handle_id = active_tab->GetHandle().raw_value();
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  contextual_tasks::ContextualTask task(task_id);
  GURL kUrl("about:blank");
  contextual_tasks::UrlResource resource(
      kUrl, contextual_tasks::ResourceType::kUnknown);
  resource.title = "about:blank";
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  // Execute full context upload compared to past tests
  // to verify full upload callback workflow.
  EXPECT_CALL(*mock_contextual_tasks_service_ptr_,
              GetContextForTask(testing::_, testing::_, testing::_, testing::_))
      .WillOnce([&context](auto, auto, auto, auto callback) {
        std::move(callback).Run(std::move(context));
      });
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillRepeatedly([session_id](auto callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        data->context_id = 123;
        data->tab_session_id = session_id;
        data->is_page_context_eligible = true;
        std::move(callback).Run(std::move(data));
      });

  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_, testing::_,
                                                  testing::_, testing::_))
      .WillRepeatedly([](int32_t, auto, auto, auto callback) {
        std::move(callback).Run(true);
      });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));

  // Run add file context's callback via mock so can store token in test.
  base::MockCallback<ContextualTasksComposeboxHandler::AddFileContextCallback>
      file_cb;
  std::optional<base::UnguessableToken> file_token_opt;

  auto file_info = searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test_file.pdf";
  file_info->mime_type = "application/pdf";
  std::vector<uint8_t> data = {0x1, 0x2};

  base::RunLoop run_loop;
  EXPECT_CALL(file_cb, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    file_token_opt = result.value();
    run_loop.Quit();
  });

  handler_->AddFileContext(std::move(file_info), mojo_base::BigBuffer(data),
                           file_cb.Get());

  run_loop.Run();

  // Run add tab context's callback via mock so can store token in test.
  base::MockCallback<ContextualTasksComposeboxHandler::AddTabContextCallback>
      normal_tab_cb;
  std::optional<base::UnguessableToken> normal_tab_token_opt;

  base::RunLoop run_loop_2;
  EXPECT_CALL(normal_tab_cb, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    normal_tab_token_opt = result.value();
    run_loop_2.Quit();
  });

  handler_->AddTabContext(tab_handle_id, /*delay_upload=*/false,
                          normal_tab_cb.Get());

  run_loop_2.Run();

  // Run add tab context's callback via mock so can store token in test.
  base::MockCallback<ContextualTasksComposeboxHandler::AddTabContextCallback>
      delayed_tab_cb;
  EXPECT_CALL(delayed_tab_cb, Run(testing::_))
      .WillOnce([&](const auto& result) {
        // We don't store the token or quit a loop for delayed_tab_cb as it's
        // not critical for the logic tested here, but we acknowledge the call.
      });
  handler_->AddTabContext(tab_handle_id, /*delay_upload=*/true,
                          delayed_tab_cb.Get());

  ASSERT_TRUE(normal_tab_token_opt.has_value());
  ASSERT_TRUE(file_token_opt.has_value());

  // Verify added context:
  ASSERT_EQ(handler_->GetNumContextUploading(), 2);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 1);
  ASSERT_TRUE(handler_->IsAnyContextUploading());

  // Configure Mock to say Files/Normal Tabs are still "Processing".
  contextual_search::FileInfo info_processing;
  info_processing.upload_status =
      contextual_search::FileUploadStatus::kProcessing;
  info_processing.tab_session_id = session_id;
  EXPECT_CALL(*mock_controller_, GetFileInfo(testing::_))
      .WillRepeatedly(testing::Return(&info_processing));

  // Do not submit to server yet.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  handler_->SubmitQuery("Combined Test", 0, false, false, false, false);
  // Delayed tabs should be uploaded once submit is run.
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);

  // Expect that message is stashed instead while normal tab/files are
  // uploading.
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 2);

  // File is finished uploading.
  handler_->OnFileUploadStatusChanged(
      *file_token_opt, lens::MimeType::kPdf,
      contextual_search::FileUploadStatus::kUploadSuccessful, std::nullopt);

  // Still waiting on Normal Tab though, so has not sent yet.
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());
  ASSERT_TRUE(handler_->IsAnyContextUploading());

  // Normal tab is finished uploading.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  handler_->OnFileUploadStatusChanged(
      *normal_tab_token_opt, lens::MimeType::kHtml,
      contextual_search::FileUploadStatus::kUploadSuccessful, std::nullopt);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       AddDeleteAdd_DelayedAndRegular_Submit) {
  // Set up task and tabs, and mock related functions.
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  int32_t tab_handle_id = active_tab->GetHandle().raw_value();
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  contextual_tasks::ContextualTask task(task_id);
  contextual_tasks::UrlResource resource(
      GURL("about:blank"), contextual_tasks::ResourceType::kUnknown);
  resource.title = "about:blank";
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  // Execute full context upload compared to past tests
  // to verify full upload callback workflow.
  EXPECT_CALL(*mock_contextual_tasks_service_ptr_,
              GetContextForTask(testing::_, testing::_, testing::_, testing::_))
      .WillRepeatedly([&context](auto, auto, auto, auto callback) {
        std::move(callback).Run(
            std::make_unique<contextual_tasks::ContextualTaskContext>(
                *context));
      });
  // Capture upload tab callback to simulate delayed tab pause.
  lens::TabContextualizationController::GetPageContextCallback
      delayed_tab_callback;
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillRepeatedly(
          [&](auto callback) { delayed_tab_callback = std::move(callback); });
  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_, testing::_,
                                                  testing::_, testing::_))
      .WillRepeatedly([](int32_t, auto, auto, auto callback) {
        std::move(callback).Run(true);
      });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));

  base::MockCallback<ContextualTasksComposeboxHandler::AddTabContextCallback>
      cb_d1;
  std::optional<base::UnguessableToken> token_d1_opt;
  base::RunLoop run_loop_d1;

  // Run add tab context's callback via mock so can store token in test.
  EXPECT_CALL(cb_d1, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    token_d1_opt = result.value();
    run_loop_d1.Quit();
  });

  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);

  handler_->AddTabContext(tab_handle_id, /*delay_upload=*/true, cb_d1.Get());
  run_loop_d1.Run();

  ASSERT_TRUE(token_d1_opt.has_value());
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 1);

  // Delete Delayed Tab #1.
  handler_->DeleteContext(*token_d1_opt, /*from_automatic_chip=*/true);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);

  // Add Delayed Tab #2 (which we will not delete).
  base::MockCallback<ContextualTasksComposeboxHandler::AddTabContextCallback>
      cb_d2;
  std::optional<base::UnguessableToken> token_d2_opt;
  base::RunLoop run_loop_d2;

  // Run add tab context's callback via mock so can store token in test.
  EXPECT_CALL(cb_d2, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    token_d2_opt = result.value();
    run_loop_d2.Quit();
  });

  handler_->AddTabContext(tab_handle_id, /*delay_upload=*/true, cb_d2.Get());
  run_loop_d2.Run();

  ASSERT_TRUE(token_d2_opt.has_value());
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 1);
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);

  // Add regular tab A (which we will delete).
  base::MockCallback<ContextualTasksComposeboxHandler::AddTabContextCallback>
      cb_rA;
  std::optional<base::UnguessableToken> token_rA_opt;
  base::RunLoop run_loop_rA;

  // Run add tab context's callback via mock so can store token in test.
  EXPECT_CALL(cb_rA, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    token_rA_opt = result.value();
    run_loop_rA.Quit();
  });

  handler_->AddTabContext(tab_handle_id, /*delay_upload=*/false, cb_rA.Get());
  run_loop_rA.Run();

  ASSERT_TRUE(token_rA_opt.has_value());
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_EQ(handler_->GetNumContextUploading(), 1);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 1);

  handler_->DeleteContext(*token_rA_opt, /*from_automatic_chip=*/false);
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 1);

  // Add regular tab B (which we will keep).
  base::MockCallback<ContextualTasksComposeboxHandler::AddTabContextCallback>
      cb_rB;
  std::optional<base::UnguessableToken> token_rB_opt;
  base::RunLoop run_loop_rB;

  // Run add tab context's callback via mock so can store token in test.
  EXPECT_CALL(cb_rB, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    token_rB_opt = result.value();
    run_loop_rB.Quit();
  });

  handler_->AddTabContext(tab_handle_id, /*delay_upload=*/false, cb_rB.Get());
  run_loop_rB.Run();

  ASSERT_TRUE(token_rB_opt.has_value());
  ASSERT_EQ(handler_->GetNumContextUploading(), 1);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 1);

  contextual_search::FileInfo info_processing;
  info_processing.upload_status =
      contextual_search::FileUploadStatus::kProcessing;
  info_processing.tab_session_id = session_id;
  EXPECT_CALL(*mock_controller_, GetFileInfo(*token_rB_opt))
      .WillRepeatedly(testing::Return(&info_processing));

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  handler_->SubmitQuery("Stress Test", 0, false, false, false, false);

  // Delayed tab #2 finishes uploading.
  ASSERT_TRUE(!delayed_tab_callback.is_null());
  auto data = std::make_unique<lens::ContextualInputData>();
  data->is_page_context_eligible = true;
  std::move(delayed_tab_callback).Run(std::move(data));

  // Verify that still uploading.
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);
  ASSERT_EQ(handler_->GetNumContextUploading(), 1);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());

  // Finish uploading file B.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  handler_->OnFileUploadStatusChanged(
      *token_rB_opt, lens::MimeType::kHtml,
      contextual_search::FileUploadStatus::kUploadSuccessful, std::nullopt);

  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_WithVisualSelection) {
  std::string kQuery = "overlay query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  base::UnguessableToken overlay_token = base::UnguessableToken::Create();

  // Set task ID so we enter the relevant if block.
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Mock GetLensOverlayToken to return a token.
  EXPECT_CALL(*handler_, GetLensOverlayToken())
      .WillOnce(testing::Return(overlay_token));

  // Expect CloseLensSync to be called.
  EXPECT_CALL(
      *mock_lens_controller_,
      CloseLensSync(
          lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted));

  // Expect GetContextForTask NOT to be called (recontextualization skipped).
  EXPECT_CALL(*mock_contextual_tasks_service_ptr_,
              GetContextForTask(testing::_, testing::_, testing::_, testing::_))
      .Times(0);

  // Expect CreateClientToAimRequest IS called (immediate submission).
  // Verify overlay token is included.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&](std::unique_ptr<
                    contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->query_text, kQuery);
        EXPECT_THAT(info->file_tokens, testing::Contains(overlay_token));
        EXPECT_TRUE(info->force_include_latest_interaction_request_data);
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_WithVisualSelection_AndUpload) {
  std::string kQuery = "overlay query with upload";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  base::UnguessableToken overlay_token = base::UnguessableToken::Create();

  // Setup an upload to make IsAnyContextUploading() true.
  auto file_info = searchbox::mojom::SelectedFileInfo::New();
  file_info->file_name = "test.pdf";
  file_info->mime_type = "application/pdf";
  std::vector<uint8_t> data = {0x1};

  // Create a pending upload
  base::MockCallback<ContextualTasksComposeboxHandler::AddFileContextCallback>
      callback;
  base::RunLoop run_loop;
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    run_loop.Quit();
  });

  handler_->AddFileContext(std::move(file_info), mojo_base::BigBuffer(data),
                           callback.Get());
  run_loop.Run();

  ASSERT_TRUE(handler_->IsAnyContextUploading());

  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  EXPECT_CALL(*handler_, GetLensOverlayToken())
      .WillOnce(testing::Return(overlay_token));

  EXPECT_CALL(
      *mock_lens_controller_,
      CloseLensSync(
          lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted));

  // Expect GetContextForTask TO be called because an upload is in progress.
  contextual_tasks::ContextualTask task(task_id);
  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(*mock_contextual_tasks_service_ptr_,
              GetContextForTask(task_id, testing::_, testing::_, testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&](std::unique_ptr<
                    contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->query_text, kQuery);
        EXPECT_THAT(info->file_tokens, testing::Contains(overlay_token));
        EXPECT_TRUE(info->force_include_latest_interaction_request_data);
        return lens::ClientToAimMessage();
      });

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  handler_->CreateAndSendQueryMessage(kQuery);

  EXPECT_TRUE(handler_->HasPendingQueryForTesting());
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_WithVisualSelection_AndUploadedTokens) {
  std::string kQuery = "overlay query with tokens";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  base::UnguessableToken overlay_token = base::UnguessableToken::Create();

  // Set task ID.
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Set up uploaded tokens in the session handle used by the handler.
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents())
      ->session_handle()
      ->CreateContextToken();

  // Mock GetLensOverlayToken.
  EXPECT_CALL(*handler_, GetLensOverlayToken())
      .WillOnce(testing::Return(overlay_token));

  // Expect CloseLensSync.
  EXPECT_CALL(
      *mock_lens_controller_,
      CloseLensSync(
          lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted));

  // Expect GetContextForTask TO BE CALLED.
  contextual_tasks::ContextualTask task(task_id);
  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(*mock_contextual_tasks_service_ptr_,
              GetContextForTask(task_id, testing::_, testing::_, testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Expect CreateClientToAimRequest to be called eventually.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&](std::unique_ptr<
                    contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->query_text, kQuery);
        EXPECT_EQ(info->file_tokens.size(), 2ul);
        EXPECT_THAT(info->file_tokens, testing::Contains(overlay_token));
        EXPECT_TRUE(info->force_include_latest_interaction_request_data);
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_NoVisualSelection) {
  std::string kQuery = "normal query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  // Token that exists but should not be used.
  base::UnguessableToken overlay_token = base::UnguessableToken::Create();

  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Mock GetLensOverlayToken to return nullopt.
  EXPECT_CALL(*handler_, GetLensOverlayToken())
      .WillOnce(testing::Return(std::nullopt));

  // Expect CloseLensSync to be called (it's always called).
  EXPECT_CALL(
      *mock_lens_controller_,
      CloseLensSync(
          lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted));

  // Expect GetContextForTask TO be called.
  contextual_tasks::ContextualTask task(task_id);
  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);
  EXPECT_CALL(*mock_contextual_tasks_service_ptr_,
              GetContextForTask(task_id, testing::_, testing::_, testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // The test returns a context with no matching attachments to the active tab,
  // so it will proceed to submission immediately.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&](std::unique_ptr<
                    contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->query_text, kQuery);
        // Verify overlay token is NOT included.
        EXPECT_THAT(info->file_tokens,
                    testing::Not(testing::Contains(overlay_token)));
        EXPECT_FALSE(info->force_include_latest_interaction_request_data);
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
}

TEST_F(ContextualTasksComposeboxHandlerTest, ClearFiles_Delayed) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "clear files query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context.
  contextual_tasks::ContextualTask task(task_id);
  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_contextual_tasks_service_ptr_,
      GetContextForTask(
          task_id,
          testing::Contains(contextual_tasks::ContextualTaskContextSource::
                                kPendingContextDecorator),
          testing::NotNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // 1. Add delayed tab context.
  tabs::TabInterface* active_tab =
      TabListInterface::From(browser())->GetActiveTab();
  int32_t active_tab_id = active_tab->GetHandle().raw_value();
  std::optional<base::UnguessableToken> token_opt;
  base::MockCallback<ContextualSearchboxHandler::AddTabContextCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    token_opt = result.value();
  });

  handler_->AddTabContext(active_tab_id, /*delay_upload=*/true, callback.Get());
  ASSERT_TRUE(token_opt.has_value());
  base::UnguessableToken token = token_opt.value();
  ASSERT_FALSE(token.is_empty());

  // 2. Clear files.
  handler_->ClearFiles(/*should_block_auto_suggested_tabs=*/false);

  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);

  // 3. Verify UploadTabContextWithData is NOT called.
  EXPECT_CALL(*handler_, UploadTabContextWithData(testing::_, testing::_,
                                                  testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
  base::RunLoop().RunUntilIdle();
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       ClearFiles_BlockAutoSuggestedTabs) {
  GURL url("https://example.com");
  auto tab_info = searchbox::mojom::TabInfo::New();
  tab_info->url = url;
  tab_info->title = "Example";

  // 1. Initially, the suggestion should be allowed.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(!received_info.is_null())
            << "Expected a non-null pointer for received_info.";
      });

  handler_->UpdateSuggestedTabContext(tab_info.Clone());

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_TRUE(handler_->has_suggested_tab_context());

  // 2. Blocklist the URL by clearing the files.
  handler_->ClearFiles(/*should_block_auto_suggested_tabs=*/true);

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_FALSE(handler_->has_suggested_tab_context());

  // 3. Simulate a title change - tab context should still be filtered out.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(received_info.is_null())
            << "Expected a null pointer for received_info.";
      });
  auto tab_info2 = searchbox::mojom::TabInfo::New();
  tab_info2->url = url;
  tab_info2->title = "Example";
  handler_->UpdateSuggestedTabContext(tab_info2.Clone());

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_FALSE(handler_->has_suggested_tab_context());
}

TEST_F(ContextualTasksComposeboxHandlerTest, UpdateSuggestedTabContext) {

  GURL url("https://example.com");
  auto tab_info = searchbox::mojom::TabInfo::New();
  tab_info->url = url;
  tab_info->title = "Example";

  // 1. Initially, the suggestion should be allowed.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(!received_info.is_null())
            << "Expected a non-null pointer for received_info.";
      });

  handler_->UpdateSuggestedTabContext(tab_info.Clone());

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_TRUE(handler_->has_suggested_tab_context());
  // 2. Blocklist the URL by dismissing an automatic chip.
  // We need to navigate the active tab to the URL being blocklisted.
  AddTab(browser(), url);

  handler_->DeleteContext(base::UnguessableToken::Create(),
                          /*from_automatic_chip=*/true);
  // 3. Now the suggestion should be filtered out.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(received_info.is_null())
            << "Expected a null pointer for received_info.";
      });

  handler_->UpdateSuggestedTabContext(tab_info.Clone());

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_FALSE(handler_->has_suggested_tab_context());
  // 4. Explicitly adding the tab should remove it from the blocklist.
  {
    base::HistogramTester histogram_tester;
    base::UserActionTester user_action_tester;
    tabs::TabInterface* active_tab =
        TabListInterface::From(browser())->GetActiveTab();
    int32_t active_tab_id = active_tab->GetHandle().raw_value();
    handler_->AddTabContext(active_tab_id, /*delay_upload=*/false,
                            base::DoNothing());
    histogram_tester.ExpectTotalCount(
        "ContextualTasks.Composebox.UserAction."
        "AddedActiveTabAfterDeletingAutoSuggestion",
        1);
    EXPECT_EQ(user_action_tester.GetActionCount(
                  "ContextualTasks.Composebox.UserAction."
                  "AddedActiveTabAfterDeletingAutoSuggestion"),
              1);
  }

  // 5. The suggestion should be allowed again.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(!received_info.is_null())
            << "Expected a non-null pointer for received_info.";
        EXPECT_EQ(received_info->url, url);
      });
  handler_->UpdateSuggestedTabContext(tab_info.Clone());

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_TRUE(handler_->has_suggested_tab_context());
}

TEST_F(ContextualTasksComposeboxHandlerTest, ResetBlocklistedSuggestions) {
  GURL url("https://example.com");
  auto tab_info = searchbox::mojom::TabInfo::New();
  tab_info->url = url;

  // 1. Blocklist the URL.
  AddTab(browser(), url);
  handler_->DeleteContext(base::UnguessableToken::Create(),
                          /*from_automatic_chip=*/true);
  // 2. Verify it's filtered out.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(received_info.is_null())
            << "Expected a null pointer for received_info.";
      });
  handler_->UpdateSuggestedTabContext(tab_info.Clone());

  searchbox_page_receiver_.FlushForTesting();
  // 3. Reset the blocklist.
  handler_->ResetBlocklistedSuggestions();

  // 4. Verify the suggestion is allowed again.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(!received_info.is_null())
            << "Expected a non-null pointer for received_info.";
        EXPECT_EQ(received_info->url, url);
      });

  handler_->UpdateSuggestedTabContext(tab_info.Clone());

  searchbox_page_receiver_.FlushForTesting();
}

TEST_F(ContextualTasksComposeboxHandlerTest, AddFileContext_NullSessionHandle) {
  // Create a handler with a callback that returns nullptr for session handle.
  mojo::PendingRemote<composebox::mojom::Page> page_remote;
  mojo::PendingReceiver<composebox::mojom::Page> page_receiver =
      page_remote.InitWithNewPipeAndPassReceiver();

  auto handler = std::make_unique<TestContextualTasksComposeboxHandler>(
      mock_ui_.get(), profile(), web_contents(),
      mojo::PendingReceiver<composebox::mojom::PageHandler>(),
      std::move(page_remote),
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      base::BindRepeating(
          []() -> contextual_search::ContextualSearchSessionHandle* {
            return nullptr;
          }),
      base::BindRepeating(&ContextualTasksUI::GetInputStateModel,
                          base::Unretained(mock_ui_.get())));
  auto file_info = searchbox::mojom::SelectedFileInfo::New();
  std::vector<uint8_t> data = {0x1};
  mojo_base::BigBuffer file_bytes(data);

  base::MockCallback<ContextualTasksComposeboxHandler::AddFileContextCallback>
      callback;
  base::expected<base::UnguessableToken, contextual_search::FileUploadErrorType>
      result;

  EXPECT_CALL(callback, Run(testing::_)).WillOnce(testing::SaveArg<0>(&result));

  handler->AddFileContext(std::move(file_info), std::move(file_bytes),
                          callback.Get());

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            contextual_search::FileUploadErrorType::kBrowserProcessingError);
}

TEST_F(ContextualTasksComposeboxHandlerTest,
       OnFileUploadStatusChanged_LensOverlayToken_Ignored) {
  base::UnguessableToken lens_token = base::UnguessableToken::Create();

  EXPECT_CALL(*mock_lens_controller_->mock_router(),
              overlay_tab_context_file_token())
      .WillRepeatedly(testing::Return(lens_token));
  EXPECT_CALL(mock_searchbox_page_, OnContextualInputStatusChanged(
                                        testing::_, testing::_, testing::_))
      .Times(0);

  handler_->OnFileUploadStatusChanged(
      lens_token, lens::MimeType::kUnknown,
      contextual_search::FileUploadStatus::kUploadSuccessful, std::nullopt);

  // Verify that for a different token, it IS called.
  base::UnguessableToken other_token = base::UnguessableToken::Create();
  EXPECT_CALL(mock_searchbox_page_, OnContextualInputStatusChanged(
                                        testing::_, testing::_, testing::_))
      .Times(1);

  handler_->OnFileUploadStatusChanged(
      other_token, lens::MimeType::kUnknown,
      contextual_search::FileUploadStatus::kUploadSuccessful, std::nullopt);
}

TEST_F(ContextualTasksComposeboxHandlerTest, ActiveModelIsPassed) {
  // 1. Arrange: Setup a mock callback to simulate ContextualTasksUI returning a
  // model. We explicitly set a distinct state (MODEL_MODE_GEMINI_PRO) to verify
  // it gets passed correctly.
  auto mock_callback = base::BindLambdaForTesting(
      [this]() -> std::unique_ptr<contextual_search::InputStateModel> {
        omnibox::SearchboxConfig config;
        auto model = std::make_unique<contextual_search::InputStateModel>(
            *session_handle_, config, false);
        model->setActiveModel(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
        return model;
      });

  mojo::PendingRemote<composebox::mojom::Page> page_remote;
  auto custom_handler = std::make_unique<TestContextualTasksComposeboxHandler>(
      mock_ui_.get(), profile(), web_contents(),
      mojo::PendingReceiver<composebox::mojom::PageHandler>(),
      std::move(page_remote),
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      base::BindRepeating(
          &ContextualTasksUI::GetOrCreateContextualSessionHandle,
          base::Unretained(mock_ui_.get())),
      std::move(mock_callback));

  // 2. Act: Trigger the handler to fetch the model via the callback.
  custom_handler->InitializeInputStateModel();

  // 3. Assert: Verify the handler successfully took ownership of the model
  // and the internal state matches exactly what the callback provided.
  contextual_search::InputStateModel* handler_model =
      custom_handler->GetInputStateModelForTesting();

  ASSERT_NE(handler_model, nullptr);
  EXPECT_EQ(handler_model->get_state_for_testing().active_model,
            omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
}
