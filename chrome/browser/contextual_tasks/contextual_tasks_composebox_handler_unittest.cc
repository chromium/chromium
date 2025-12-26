// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_context_controller.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
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
  contextual_tasks::ContextualTasksContextController* GetContextController()
      override {
    return mock_controller_;
  }

 public:
  void SetMockController(
      contextual_tasks::ContextualTasksContextController* controller) {
    mock_controller_ = controller;
  }

 private:
  raw_ptr<contextual_tasks::ContextualTasksContextController> mock_controller_ =
      nullptr;
};

class ContextualTasksComposeboxHandlerTest
    : public LocalContextualSearchboxHandlerTestHarness {
 public:
  ContextualTasksComposeboxHandlerTest() = default;
  ~ContextualTasksComposeboxHandlerTest() override = default;

  void SetUp() override {
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
        ->set_session_handle(std::move(contextual_session_handle));

    mock_ui_ =
        std::make_unique<testing::NiceMock<MockContextualTasksUI>>(&web_ui_);
    ON_CALL(*mock_ui_, GetWebUIWebContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(*mock_ui_, GetTaskId())
        .WillByDefault(testing::ReturnRefOfCopy(std::optional<base::Uuid>()));

    // Create mock controller directly.
    mock_tasks_context_controller_owner_ = std::make_unique<testing::NiceMock<
        contextual_tasks::MockContextualTasksContextController>>();
    mock_tasks_controller_ptr_ = mock_tasks_context_controller_owner_.get();

    handler_ = std::make_unique<TestContextualTasksComposeboxHandler>(
        mock_ui_.get(), profile(), web_contents(),
        mojo::PendingReceiver<composebox::mojom::PageHandler>(),
        mojo::PendingRemote<composebox::mojom::Page>(),
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        base::BindRepeating(
            &ContextualTasksUI::GetOrCreateContextualSessionHandle,
            base::Unretained(mock_ui_.get())));
    handler_->SetMockController(mock_tasks_controller_ptr_);

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
  }

  std::unique_ptr<contextual_tasks::MockContextualTasksContextController>
      mock_tasks_context_controller_owner_;

  void TearDown() override {
    handler_.reset();
    mock_controller_ = nullptr;
    mock_tasks_controller_ptr_ = nullptr;
    mock_tab_controller_ = nullptr;
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
  raw_ptr<contextual_tasks::MockContextualTasksContextController>
      mock_tasks_controller_ptr_ = nullptr;

  raw_ptr<MockTabContextualizationController> mock_tab_controller_ = nullptr;
};

TEST_F(ContextualTasksComposeboxHandlerTest, SubmitQuery) {
  ASSERT_NE(mock_tasks_controller_ptr_, nullptr)
      << "Mock controller is NULL in SubmitQuery!";
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->SubmitQuery("test query", 0, false, false, false, false);
}

TEST_F(ContextualTasksComposeboxHandlerTest, CreateAndSendQueryMessage) {
  std::string kQuery = "direct query";
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillOnce(testing::ReturnRefOfCopy(std::optional<base::Uuid>()));
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
  ASSERT_NE(mock_tasks_controller_ptr_, nullptr) << "Mock controller is NULL!";
  std::string kQuery = "recontextualize query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillOnce(testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context with expired tab.
  contextual_tasks::ContextualTask task(task_id);
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  contextual_tasks::UrlResource resource(GURL("http://example.com"));
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_tasks_controller_ptr_,
      GetContextForTask(task_id, testing::_, testing::IsNull(), testing::_))
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
      .WillOnce(
          [this](MockTabContextualizationController::GetPageContextCallback
                     callback) {
            // Return some dummy content.
            auto data = std::make_unique<lens::ContextualInputData>();
            data->context_input = std::vector<lens::ContextualInput>();
            data->tab_session_id = sessions::SessionTabHelper::IdForTab(
                browser()->tab_strip_model()->GetActiveWebContents());
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
  ASSERT_NE(mock_tasks_controller_ptr_, nullptr) << "Mock controller is NULL!";
  std::string kQuery = "recontextualize query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillOnce(testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context with uploaded tab (not expired).
  contextual_tasks::ContextualTask task(task_id);
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  contextual_tasks::UrlResource resource(GURL("http://example.com"));
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_tasks_controller_ptr_,
      GetContextForTask(task_id, testing::_, testing::IsNull(), testing::_))
      .WillOnce(
          [&context](
              const base::Uuid& task_id,
              const std::set<contextual_tasks::ContextualTaskContextSource>&
                  sources,
              std::unique_ptr<contextual_tasks::ContextDecorationParams> params,
              base::OnceCallback<void(
                  std::unique_ptr<contextual_tasks::ContextualTaskContext>)>
                  callback) { std::move(callback).Run(std::move(context)); });

  // Setup FileInfo with uploaded status and some previous content.
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
      .WillOnce([this](
                    MockTabContextualizationController::GetPageContextCallback
                        callback) {
        auto data = std::make_unique<lens::ContextualInputData>();
        std::string new_content = "new content";
        std::vector<uint8_t> new_bytes(new_content.begin(), new_content.end());
        lens::ContextualInput new_input(std::move(new_bytes),
                                        lens::MimeType::kPlainText);
        data->context_input =
            std::vector<lens::ContextualInput>{std::move(new_input)};
        data->tab_session_id = sessions::SessionTabHelper::IdForTab(
            browser()->tab_strip_model()->GetActiveWebContents());
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
       CreateAndSendQueryMessage_AlwaysRecontextualizes) {
  ASSERT_NE(mock_tasks_controller_ptr_, nullptr) << "Mock controller is NULL!";
  std::string kQuery = "valid tab query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillOnce(testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context with uploaded tab
  contextual_tasks::ContextualTask task(task_id);
  SessionID session_id = sessions::SessionTabHelper::IdForTab(web_contents());
  contextual_tasks::UrlResource resource(GURL("http://example.com"));
  resource.tab_id = session_id;
  task.AddUrlResource(resource);

  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  EXPECT_CALL(
      *mock_tasks_controller_ptr_,
      GetContextForTask(task_id, testing::_, testing::IsNull(), testing::_))
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
  file_info_list.push_back(&file_info);

  EXPECT_CALL(*mock_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  // Expect GetPageContext call with SAME content.
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillOnce(
          [this](MockTabContextualizationController::GetPageContextCallback
                     callback) {
            auto data = std::make_unique<lens::ContextualInputData>();
            // Reconstruct same input.
            std::string content = "same content";
            std::vector<uint8_t> bytes(content.begin(), content.end());
            lens::ContextualInput new_input(std::move(bytes),
                                            lens::MimeType::kPlainText);
            data->context_input =
                std::vector<lens::ContextualInput>{std::move(new_input)};
            // Set session ID to match
            data->tab_session_id = sessions::SessionTabHelper::IdForTab(
                browser()->tab_strip_model()->GetActiveWebContents());
            std::move(callback).Run(std::move(data));
          });

  // Expect UploadTabContextWithData call because we ALWAYS recontextualize now.
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
  // Currently this method does nothing, but we verify it can be called safely.
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
