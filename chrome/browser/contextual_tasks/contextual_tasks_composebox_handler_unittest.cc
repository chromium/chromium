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
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
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
    web_contents_ = browser()->tab_strip_model()->GetActiveWebContents();
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
  MOCK_METHOD(void, DisableActiveTabContextSuggestion, (), (override));
  MOCK_METHOD(BrowserWindowInterface*, GetBrowser, (), (override));
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

 private:
  raw_ptr<contextual_tasks::ContextualTasksService>
      mock_contextual_tasks_service_ = nullptr;
};

class MockLensSearchController : public LensSearchController {
 public:
  explicit MockLensSearchController(tabs::TabInterface* tab)
      : LensSearchController(tab) {}
  ~MockLensSearchController() override = default;

  MOCK_METHOD(void,
              OpenLensOverlay,
              (lens::LensOverlayInvocationSource invocation_source),
              (override));
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
        service_->GetSession(contextual_session_handle->session_id());
    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents())
        ->SetTaskSession(std::nullopt, std::move(contextual_session_handle));

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
            base::Unretained(mock_ui_.get())));
    handler_->SetMockContextualTasksService(mock_contextual_tasks_service_ptr_);

    // Setup MockTabContextualizationController
    tabs::TabInterface* active_tab =
        browser()->tab_strip_model()->GetActiveTab();
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

  contextual_tasks::UrlResource resource(kUrl);
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
  file_info.request_id.set_context_id(12345);
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

  contextual_tasks::UrlResource resource(kUrl);
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
  file_info.request_id.set_context_id(12345);

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
  contextual_tasks::UrlResource resource(GURL("about:blank"));
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
  file_info.request_id.set_context_id(12345);

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
  contextual_tasks::UrlResource resource(GURL("http://example.com"));
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

  contextual_tasks::UrlResource resource(kUrl);
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

  contextual_tasks::UrlResource resource(kUrl);
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
  file_info.request_id.set_context_id(12345);

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

TEST_F(ContextualTasksComposeboxHandlerTest,
       CreateAndSendQueryMessage_NoRecontextualizationIfScreenshotUnchanged_SkBitmap) {
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
  contextual_tasks::UrlResource resource(GURL("about:blank"));
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
  file_info.request_id.set_context_id(12345);

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
          lens::LensOverlayInvocationSource::kContextualTasksComposebox));
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
  omnibox::ChromeAimToolsAndModels tool_mode;
  bool expected_deep_search_selected;
  bool expected_create_images_selected;
};

class ContextualTasksComposeboxHandlerToolModeTest
    : public ContextualTasksComposeboxHandlerTest,
      public ::testing::WithParamInterface<ToolModeTestParam> {};

TEST_P(ContextualTasksComposeboxHandlerToolModeTest, SetsToolModeFlags) {
  const auto& param = GetParam();

  if (param.tool_mode ==
      omnibox::ChromeAimToolsAndModels::TOOL_MODE_DEEP_SEARCH) {
    handler_->SetDeepSearchMode(true);
  } else if (param.tool_mode ==
             omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN) {
    handler_->SetCreateImageMode(true, false);
  } else if (param.tool_mode ==
             omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN_UPLOAD) {
    handler_->SetCreateImageMode(true, true);
  }

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&](std::unique_ptr<
                    contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->deep_search_selected,
                  param.expected_deep_search_selected);
        EXPECT_EQ(info->create_images_selected,
                  param.expected_create_images_selected);
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage("test query");
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksComposeboxHandlerToolModeTest,
    ::testing::Values(
        ToolModeTestParam{
            omnibox::ChromeAimToolsAndModels::TOOL_MODE_UNSPECIFIED, false,
            false},
        ToolModeTestParam{
            omnibox::ChromeAimToolsAndModels::TOOL_MODE_DEEP_SEARCH, true,
            false},
        ToolModeTestParam{omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN,
                          false, true},
        ToolModeTestParam{
            omnibox::ChromeAimToolsAndModels::TOOL_MODE_IMAGE_GEN_UPLOAD, false,
            true}));

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

  // 1. Add delayed tab context.
  int32_t tab_id = 100;
  std::optional<base::UnguessableToken> token_opt;
  base::MockCallback<ContextualSearchboxHandler::AddTabContextCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce(testing::SaveArg<0>(&token_opt));

  handler_->AddTabContext(tab_id, /*delay_upload=*/true, callback.Get());
  ASSERT_TRUE(token_opt.has_value());
  base::UnguessableToken token = token_opt.value();
  ASSERT_FALSE(token.is_empty());

  // 2. Verify tab is added to GetTabsToUpdate (via CreateAndSendQueryMessage).
  // We need to mock the tab handle resolution. Since we can't easily mock
  // TabHandle::Get() for arbitrary IDs in this test harness without more setup,
  // we will use the active tab's ID which IS set up.
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  int32_t active_tab_id = active_tab->GetHandle().raw_value();

  // Reset and try again with active tab ID.
  std::optional<base::UnguessableToken> active_token_opt;
  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce(testing::SaveArg<0>(&active_token_opt));
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
  tabs::TabInterface* active_tab = browser()->tab_strip_model()->GetActiveTab();
  int32_t active_tab_id = active_tab->GetHandle().raw_value();
  std::optional<base::UnguessableToken> token_opt;
  base::MockCallback<ContextualSearchboxHandler::AddTabContextCallback>
      callback;
  EXPECT_CALL(callback, Run(testing::_))
      .WillOnce(testing::SaveArg<0>(&token_opt));

  handler_->AddTabContext(active_tab_id, /*delay_upload=*/true, callback.Get());
  ASSERT_TRUE(token_opt.has_value());
  base::UnguessableToken token = token_opt.value();
  ASSERT_FALSE(token.is_empty());

  // 2. Delete the context.
  handler_->DeleteContext(token, /*from_automatic_chip=*/true);

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
