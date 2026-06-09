// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/contextual_search/desktop_query_contextualizer_delegate.h"
#include "chrome/browser/ui/contextual_search/tab_contextualization_controller.h"
#include "chrome/browser/ui/lens/lens_query_flow_router.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/browser/ui/webui/webui_embedding_context.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/fake_variations_client.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/contextual_search/mock_contextual_search_session_handle.h"
#include "components/contextual_tasks/public/contextual_task.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/omnibox/common/composebox_features.h"
#include "components/sessions/content/session_tab_helper.h"
#include "components/tabs/public/mock_tab_interface.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
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

class LocalContextualSearchboxHandlerTestHarness : public InProcessBrowserTest {
 public:
  LocalContextualSearchboxHandlerTestHarness() = default;
  ~LocalContextualSearchboxHandlerTestHarness() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    tabs::TabInterface* active_tab = AddTab(GURL(url::kAboutBlankURL));
    web_contents_ = active_tab->GetContents();

    template_url_service_ = TemplateURLServiceFactory::GetForProfile(profile());
  }

  void TearDownOnMainThread() override {
    // Safely reset pointers inside controllers to avoid dangling references
    for (int i = 0; i < browser()->tab_strip_model()->count(); ++i) {
      tabs::TabInterface* tab =
          tabs::TabLookupFromWebContents::FromWebContents(
              browser()->tab_strip_model()->GetWebContentsAt(i))
              ->model();
      if (tab && tab->GetTabFeatures()) {
        tab->GetTabFeatures()->SetTabContextualizationControllerForTesting(
            nullptr);
      }
    }
    mock_tab_controller_ = nullptr;
    web_contents_ = nullptr;
    template_url_service_ = nullptr;
    tab_features_override_ = nullptr;
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  raw_ptr<content::WebContents> web_contents_ = nullptr;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  contextual_search::FakeVariationsClient fake_variations_client_;
  raw_ptr<tabs::TabFeatures> tab_features_override_ = nullptr;

  // Mock controller kept by Setup/AddTab to set expectations
  raw_ptr<MockTabContextualizationController> mock_tab_controller_ = nullptr;

  // Helper methods to access protected members
  content::WebContents* web_contents() { return web_contents_; }
  Profile* profile() { return browser()->profile(); }
  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory() {
    return profile()
        ->GetDefaultStoragePartition()
        ->GetURLLoaderFactoryForBrowserProcess();
  }
  TemplateURLService* template_url_service() { return template_url_service_; }
  contextual_search::FakeVariationsClient* fake_variations_client() {
    return &fake_variations_client_;
  }

  tabs::TabInterface* AddTab(const GURL& url) {
    chrome::AddSelectedTabWithURL(browser(), url, ui::PAGE_TRANSITION_LINK);
    content::WebContents* contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver navigation_observer(contents);
    navigation_observer.Wait();

    tabs::TabInterface* tab =
        tabs::TabLookupFromWebContents::FromWebContents(contents)->model();
    tab->GetTabFeatures()->SetTabContextualizationControllerForTesting(nullptr);
    auto mock_tab_controller =
        std::make_unique<MockTabContextualizationController>(tab);
    mock_tab_controller_ = mock_tab_controller.get();
    tab->GetTabFeatures()->SetTabContextualizationControllerForTesting(
        std::move(mock_tab_controller));

    return tab;
  }
};

class MockContextualTasksUI : public ContextualTasksUI {
 public:
  explicit MockContextualTasksUI(content::WebUI* web_ui)
      : ContextualTasksUI(web_ui) {}
  ~MockContextualTasksUI() override = default;

  contextual_search::ContextualSearchSessionHandle*
  GetOrCreateContextualSessionHandle() override {
    return session_handle_ptr_;
  }
  void SetSessionHandle(
      contextual_search::ContextualSearchSessionHandle* handle) {
    session_handle_ptr_ = handle;
  }
  MOCK_METHOD(void,
              PostMessageToWebview,
              (const lens::ClientToAimMessage& message),
              (override));
  MOCK_METHOD(content::WebContents*, GetWebUIWebContents, (), (override));
  MOCK_METHOD(const std::optional<base::Uuid>&, GetTaskId, (), (override));
  MOCK_METHOD(BrowserWindowInterface*, GetBrowser, (), (override));
  MOCK_METHOD(bool, IsLensOverlayShowing, (), (const, override));
  MOCK_METHOD(const GURL&, GetInnerFrameUrl, (), (const, override));
  MOCK_METHOD(std::unique_ptr<contextual_search::InputStateModel>,
              TakeInputStateModel,
              (),
              (override));
  MOCK_METHOD(std::vector<int32_t>, GetRestoredTabIds, (), (override));
  MOCK_METHOD(bool, IsActiveTabContextSuggestionShowing, (), (const, override));
  MOCK_METHOD(contextual_tasks::ContextualTasksAutoSuggestionManager*,
              GetAutoSuggestionManager,
              (),
              (override));

 private:
  raw_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_ptr_ = nullptr;
};

class TestContextualTasksComposeboxHandler
    : public ContextualTasksComposeboxHandler {
 public:
  using ContextualTasksComposeboxHandler::ContextualTasksComposeboxHandler;

  MOCK_METHOD(std::optional<base::UnguessableToken>,
              GetLensOverlayToken,
              (),
              (override));
  MOCK_METHOD(LensSearchController*,
              GetLensSearchController,
              (),
              (const, override));
  MOCK_METHOD(void,
              OnContextUploadStatusChanged,
              (const base::UnguessableToken& context_token,
               lens::MimeType mime_type,
               contextual_search::ContextUploadStatus context_upload_status,
               const std::optional<contextual_search::ContextUploadErrorType>&
                   error_type),
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

  contextual_search::InputStateModel* TakeInputStateModelForTesting() {
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
  MOCK_METHOD(void,
              CloseLensAsync,
              (lens::LensOverlayDismissalSource dismissal_source),
              (override));
  MOCK_METHOD(void,
              CloseLensAsync,
              (lens::LensOverlayDismissalSource dismissal_source,
               bool side_panel_already_closing),
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
  ContextualTasksComposeboxHandlerTest()
      : ContextualTasksComposeboxHandlerTest(
            std::map<std::string, std::string>()) {}

  explicit ContextualTasksComposeboxHandlerTest(
      const std::map<std::string, std::string>& parameters) {
    feature_list_.InitAndEnableFeatureWithParameters(
        contextual_tasks::kContextualTasks, parameters);
  }
  ~ContextualTasksComposeboxHandlerTest() override = default;

  void SimulateUploadStatusChanged(
      const base::UnguessableToken& context_token,
      lens::MimeType mime_type,
      contextual_search::ContextUploadStatus context_upload_status,
      const std::optional<contextual_search::ContextUploadErrorType>&
          error_type = std::nullopt) {
    for (auto& obs : upload_observers_) {
      obs.OnContextUploadStatusChanged(context_token, mime_type,
                                       context_upload_status, error_type);
    }
  }

  void PostUploadStatusChanged(
      const base::UnguessableToken& context_token,
      lens::MimeType mime_type,
      contextual_search::ContextUploadStatus context_upload_status,
      const std::optional<contextual_search::ContextUploadErrorType>&
          error_type = std::nullopt) {
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(
            &ContextualTasksComposeboxHandlerTest::SimulateUploadStatusChanged,
            base::Unretained(this), context_token, mime_type,
            context_upload_status, error_type));
  }

  std::unique_ptr<contextual_search::InputStateModel>
  CreateMockInputStateModel() {
    omnibox::SearchboxConfig config;
    auto model = std::make_unique<contextual_search::InputStateModel>(
        *session_handle_, config, GURL(), /*is_off_the_record=*/false,
        /*is_signed_in=*/false);
    model->setActiveModel(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
    return model;
  }

  void SetUpMockUI() {
    web_ui_.set_web_contents(web_contents());
    webui::SetTabInterface(web_contents(), nullptr);
    webui::SetBrowserWindowInterface(web_contents(), browser());

    auto mock_controller = std::make_unique<testing::NiceMock<
        contextual_search::MockContextualSearchContextController>>();
    mock_controller_ = mock_controller.get();

    controller_weak_factory_ = std::make_unique<base::WeakPtrFactory<
        contextual_search::ContextualSearchContextController>>(
        mock_controller_);
    ON_CALL(*mock_controller_, AsWeakPtr()).WillByDefault([this]() {
      return controller_weak_factory_->GetWeakPtr();
    });

    ON_CALL(*mock_controller_, AddObserver(testing::_))
        .WillByDefault(
            [this](contextual_search::ContextualSearchContextController::
                       ContextUploadStatusObserver* obs) {
              if (!upload_observers_.HasObserver(obs)) {
                upload_observers_.AddObserver(obs);
              }
            });
    ON_CALL(*mock_controller_, RemoveObserver(testing::_))
        .WillByDefault(
            [this](contextual_search::ContextualSearchContextController::
                       ContextUploadStatusObserver* obs) {
              upload_observers_.RemoveObserver(obs);
            });
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
    session_handle_->CheckSearchContentSharingSettings(profile()->GetPrefs());
    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents())
        ->SetTaskSession(std::nullopt, std::move(contextual_session_handle),
                         /*input_state_model=*/nullptr);

    mock_ui_ =
        std::make_unique<testing::NiceMock<MockContextualTasksUI>>(&web_ui_);
    mock_ui_->SetSessionHandle(session_handle_.get());
    ON_CALL(*mock_ui_, GetWebUIWebContents())
        .WillByDefault(testing::Return(web_contents()));
    ON_CALL(*mock_ui_, GetTaskId())
        .WillByDefault(testing::ReturnRefOfCopy(std::optional<base::Uuid>()));
    ON_CALL(*mock_ui_, GetBrowser()).WillByDefault(testing::Return(browser()));
    ON_CALL(*mock_ui_, GetInnerFrameUrl())
        .WillByDefault(testing::ReturnRefOfCopy(GURL()));
    ON_CALL(*mock_ui_, GetAutoSuggestionManager())
        .WillByDefault(testing::Return(&auto_suggestion_manager_));
    ON_CALL(*mock_ui_, IsActiveTabContextSuggestionShowing())
        .WillByDefault([this]() {
          return auto_suggestion_manager_.GetCurrentSuggestion() != nullptr;
        });

    // Create mock controller directly.
    mock_contextual_tasks_service_owner_ = std::make_unique<
        testing::NiceMock<contextual_tasks::MockContextualTasksService>>();
    mock_contextual_tasks_service_ptr_ =
        mock_contextual_tasks_service_owner_.get();
  }

  void SetUpHandler() {
    mojo::PendingRemote<composebox::mojom::Page> page_remote;
    page_receiver_ = page_remote.InitWithNewPipeAndPassReceiver();

    handler_ = std::make_unique<TestContextualTasksComposeboxHandler>(
        mock_ui_.get(), profile(), web_contents(),
        mojo::PendingReceiver<composebox::mojom::PageHandler>(),
        std::move(page_remote),
        mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
        searchbox_page_receiver_.BindNewPipeAndPassRemote(),
        base::BindRepeating(
            &ContextualTasksUI::GetOrCreateContextualSessionHandle,
            base::Unretained(mock_ui_.get())),
        base::BindRepeating(&ContextualTasksUI::ClearContextualSessionHandle,
                            base::Unretained(mock_ui_.get())),
        base::BindRepeating(&ContextualTasksUI::TakeInputStateModel,
                            base::Unretained(mock_ui_.get())));
    ON_CALL(*handler_, GetLensSearchController())
        .WillByDefault(testing::Return(mock_lens_controller_.get()));
    handler_->SetMockContextualTasksService(mock_contextual_tasks_service_ptr_);
    handler_->recontextualizer_ =
        std::make_unique<contextual_tasks::QueryContextualizer>(
            mock_contextual_tasks_service_ptr_,
            handler_->desktop_delegate_.get());

    // Default to calling the real implementation for
    // OnContextUploadStatusChanged.
    ON_CALL(*handler_, OnContextUploadStatusChanged(testing::_, testing::_,
                                                    testing::_, testing::_))
        .WillByDefault(
            [handler = handler_.get()](
                const base::UnguessableToken& context_token,
                lens::MimeType mime_type,
                contextual_search::ContextUploadStatus context_upload_status,
                const std::optional<contextual_search::ContextUploadErrorType>&
                    error_type) {
              handler->ContextualTasksComposeboxHandler::
                  OnContextUploadStatusChanged(context_token, mime_type,
                                               context_upload_status,
                                               error_type);
            });

    ASSERT_TRUE(mock_lens_controller_);
  }

  void SetUpOnMainThread() override {
    // Install override before AddTab is called in base SetUpOnMainThread.
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

    LocalContextualSearchboxHandlerTestHarness::SetUpOnMainThread();
    SetUpMockUI();
    SetUpHandler();
  }

  std::unique_ptr<contextual_tasks::MockContextualTasksService>
      mock_contextual_tasks_service_owner_;

  void TearDownOnMainThread() override {
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
    mock_ui_->SetSessionHandle(nullptr);
    session_handle_.reset();
    service_.reset();
    mock_ui_.reset();
    LocalContextualSearchboxHandlerTestHarness::TearDownOnMainThread();
  }

 protected:
  content::TestWebUI web_ui_;
  std::unique_ptr<testing::NiceMock<MockContextualTasksUI>> mock_ui_;
  contextual_tasks::ContextualTasksAutoSuggestionManager
      auto_suggestion_manager_;
  std::unique_ptr<TestContextualTasksComposeboxHandler> handler_;

  // For session management.
  std::unique_ptr<contextual_search::ContextualSearchService> service_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  raw_ptr<contextual_search::MockContextualSearchContextController>
      mock_controller_;
  raw_ptr<contextual_tasks::MockContextualTasksService>
      mock_contextual_tasks_service_ptr_ = nullptr;

  raw_ptr<MockLensSearchController> mock_lens_controller_ = nullptr;

  testing::NiceMock<MockSearchboxPage> mock_searchbox_page_;
  mojo::Receiver<searchbox::mojom::Page> searchbox_page_receiver_{
      &mock_searchbox_page_};

  base::ObserverList<contextual_search::ContextualSearchContextController::
                         ContextUploadStatusObserver>
      upload_observers_;
  std::unique_ptr<base::WeakPtrFactory<
      contextual_search::ContextualSearchContextController>>
      controller_weak_factory_;

  base::test::ScopedFeatureList feature_list_;
  ui::UserDataFactory::ScopedOverride lens_controller_override_;
  mojo::PendingReceiver<composebox::mojom::Page> page_receiver_;
};

class ContextualTasksComposeboxHandlerTestWithAutoSuggestionDisabled
    : public ContextualTasksComposeboxHandlerTest {
 public:
  ContextualTasksComposeboxHandlerTestWithAutoSuggestionDisabled()
      : ContextualTasksComposeboxHandlerTest(
            {{"ContextualTasksTabAutoSuggestionChipEnabled", "false"}}) {}
  ~ContextualTasksComposeboxHandlerTestWithAutoSuggestionDisabled() override =
      default;
};

class ContextualTasksComposeboxHandlerTestWithContextManagementEnabled
    : public ContextualTasksComposeboxHandlerTest {
 public:
  ContextualTasksComposeboxHandlerTestWithContextManagementEnabled() {
    feature_list_context_management_.InitAndEnableFeature(
        omnibox::kContextManagementInComposebox);
  }
  ~ContextualTasksComposeboxHandlerTestWithContextManagementEnabled() override =
      default;

  void SetUpOnMainThread() override {
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

    LocalContextualSearchboxHandlerTestHarness::SetUpOnMainThread();
    SetUpMockUI();
  }

 private:
  base::test::ScopedFeatureList feature_list_context_management_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest, SubmitQuery) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL in SubmitQuery!";
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));
  EXPECT_CALL(
      *mock_lens_controller_,
      CloseLensSync(
          lens::LensOverlayDismissalSource::kContextualTasksQuerySubmitted));

  handler_->SubmitQuery("test query", 0, false, false, false, false,
                        /*is_voice_search=*/false);
  EXPECT_EQ(session_handle_->previous_turns().back().query, "test query");
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       CloseLensOverlayFromWebUI) {
  EXPECT_CALL(*mock_lens_controller_,
              CloseLensAsync(lens::LensOverlayDismissalSource::
                                 kContextualTasksImageUploadsDisabled));

  handler_->CloseLensOverlayFromWebUI(
      composebox::mojom::LensOverlayDismissalSource::
          kContextualTasksImageUploadsDisabled);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       CreateAndSendQueryMessage) {
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

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       CreateAndSendQueryMessage_UpdatesMetricsRecorderSource) {
  // Set the initial source of the session metrics recorder to kLens.
  session_handle_->GetMetricsRecorder()->UpdateContextualSearchSource(
      contextual_search::ContextualSearchSource::kLens);
  EXPECT_EQ(session_handle_->GetMetricsRecorder()->source(),
            contextual_search::ContextualSearchSource::kLens);

  std::string kQuery = "direct query";
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(testing::ReturnRefOfCopy(std::optional<base::Uuid>()));
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);

  // The source of the metrics recorder should now be updated to
  // kContextualTasks.
  EXPECT_EQ(session_handle_->GetMetricsRecorder()->source(),
            contextual_search::ContextualSearchSource::kContextualTasks);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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
                                kSubmittedContextDecorator),
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
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadExpired;
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

  // Expect StartFileUploadFlow call.
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce(
          [this](const base::UnguessableToken& file_token,
                 std::unique_ptr<lens::ContextualInputData> data,
                 std::optional<lens::ImageEncodingOptions> image_options) {
            EXPECT_TRUE(data->is_implicit_upload);
            PostUploadStatusChanged(
                file_token, lens::MimeType::kUnknown,
                contextual_search::ContextUploadStatus::kUploadSuccessful);
          });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksComposeboxHandlerTest,
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
                                kSubmittedContextDecorator),
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
      contextual_search::ContextUploadStatus::kUploadSuccessful;
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

  // Expect StartFileUploadFlow call because content changed.
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce(
          [this](const base::UnguessableToken& file_token,
                 std::unique_ptr<lens::ContextualInputData> data,
                 std::optional<lens::ImageEncodingOptions> image_options) {
            EXPECT_TRUE(data->is_implicit_upload);
            PostUploadStatusChanged(
                file_token, lens::MimeType::kUnknown,
                contextual_search::ContextUploadStatus::kUploadSuccessful);
          });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksComposeboxHandlerTest,
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
                                kSubmittedContextDecorator),
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
      contextual_search::ContextUploadStatus::kUploadSuccessful;
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

  // Expect StartFileUploadFlow call to NOT be called.
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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
                                kSubmittedContextDecorator),
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

  // Expect StartFileUploadFlow to NOT be called.
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  // Expect CreateClientToAimRequest IS called (query submission continues).
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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
                                kSubmittedContextDecorator),
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

  // Expect StartFileUploadFlow to NOT be called.
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  // Expect CreateClientToAimRequest IS called.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
  run_loop.Run();
}

// crbug.com/488112121: This test covers the temporary behavior of disabling
// tools when the aegc=1 URL parameter is present. Remove this test when the
// temporary workaround in ContextualTasksComposeboxHandler is removed.
IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       AegcParameterDisablesTools) {
  omnibox::SearchboxConfig config;
  config.add_input_type_configs()->set_input_type(
      omnibox::InputType::INPUT_TYPE_LENS_IMAGE);
  config.add_input_type_configs()->set_input_type(
      omnibox::InputType::INPUT_TYPE_LENS_FILE);
  config.add_tool_configs()->set_tool(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH);

  auto session_handle =
      std::make_unique<contextual_search::MockContextualSearchSessionHandle>();
  auto input_state_model = std::make_unique<contextual_search::InputStateModel>(
      *session_handle, config, GURL(), /*is_off_the_record=*/false,
      /*is_signed_in=*/false);

  EXPECT_CALL(*mock_ui_, TakeInputStateModel())
      .WillOnce(testing::Return(testing::ByMove(std::move(input_state_model))));

  GURL aegc_url("https://gemini.google.com/app?aegc=1");
  EXPECT_CALL(*mock_ui_, GetInnerFrameUrl())
      .WillRepeatedly(testing::ReturnRef(aegc_url));

  // Re-initialize the model to pick up the URL change.
  handler_->OnTaskChanged();

  auto* model = handler_->TakeInputStateModelForTesting();
  ASSERT_TRUE(model);

  const auto& state = model->get_state_for_testing();

  EXPECT_THAT(state.disabled_tools,
              testing::Contains(omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH));

  EXPECT_THAT(
      state.disabled_input_types,
      testing::UnorderedElementsAre(omnibox::InputType::INPUT_TYPE_LENS_FILE));
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksComposeboxHandlerTest,
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
                                kSubmittedContextDecorator),
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
      contextual_search::ContextUploadStatus::kUploadSuccessful;
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

  // Expect StartFileUploadFlow call because bitmap changed.
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce(
          [this](const base::UnguessableToken& context_token,
                 std::unique_ptr<lens::ContextualInputData> data,
                 std::optional<lens::ImageEncodingOptions> image_options) {
            EXPECT_TRUE(data->is_implicit_upload);
            PostUploadStatusChanged(
                context_token, lens::MimeType::kUnknown,
                contextual_search::ContextUploadStatus::kUploadSuccessful);
          });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
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
                                kSubmittedContextDecorator),
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
      contextual_search::ContextUploadStatus::kUploadSuccessful;
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

  // Expect StartFileUploadFlow call to NOT be called.
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       OnAutocompleteAccept) {
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

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       HandleLensButtonClick) {
  EXPECT_CALL(
      *mock_lens_controller_,
      OpenLensOverlay(
          lens::LensOverlayInvocationSource::kContextualTasksComposebox, true));
  handler_->HandleLensButtonClick();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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

IN_PROC_BROWSER_TEST_P(ContextualTasksComposeboxHandlerToolModeTest,
                       SetsToolModeFlags) {
  const auto& param = GetParam();

  handler_->SetActiveToolMode(param.tool_mode);
  handler_->RecordToolSelectionAction(param.tool_mode);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&](std::unique_ptr<
                    contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->active_tool, param.tool_mode);
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage("test query", /*is_voice_search=*/false);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    ContextualTasksComposeboxHandlerToolModeTest,
    ::testing::Values(
        ToolModeTestParam{omnibox::ToolMode::TOOL_MODE_UNSPECIFIED},
        ToolModeTestParam{omnibox::ToolMode::TOOL_MODE_DEEP_SEARCH},
        ToolModeTestParam{omnibox::ToolMode::TOOL_MODE_IMAGE_GEN},
        ToolModeTestParam{omnibox::ToolMode::TOOL_MODE_IMAGE_GEN_UPLOAD}));

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       AddTabContext_Delayed) {
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
                                kSubmittedContextDecorator),
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
  std::vector<int32_t> selected_tab_ids = handler_->GetSelectedTabIds();
  EXPECT_THAT(selected_tab_ids, testing::Contains(tab_id));
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

  // Expect StartFileUploadFlow call.
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce(
          [this](const base::UnguessableToken& file_token,
                 std::unique_ptr<lens::ContextualInputData> data,
                 std::optional<lens::ImageEncodingOptions> image_options) {
            // The delay-upload tab is an implicit upload because it was
            // auto-suggested.
            EXPECT_TRUE(data->is_implicit_upload);
            PostUploadStatusChanged(
                file_token, lens::MimeType::kUnknown,
                contextual_search::ContextUploadStatus::kUploadSuccessful);
          });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
  run_loop.Run();

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       DeleteContext_Delayed) {
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
                                kSubmittedContextDecorator),
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

  // 3. Verify StartFileUploadFlow is NOT called when submitting.
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksComposeboxHandlerTestWithContextManagementEnabled,
    RestoreTabIds) {
  std::vector<int32_t> restored_tab_ids = {1, 2};
  EXPECT_CALL(*mock_ui_, GetRestoredTabIds())
      .WillOnce(testing::Return(restored_tab_ids));
  EXPECT_CALL(mock_searchbox_page_, SetRestoredTabIds(restored_tab_ids))
      .Times(1);

  SetUpHandler();
  searchbox_page_receiver_.FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       GetSelectedTabIds) {
  int32_t tab_id = 15;
  base::MockCallback<ContextualSearchboxHandler::AddTabContextCallback>
      callback;
  handler_->AddTabContext(tab_id, /*delay_upload=*/true, callback.Get());

  std::vector<int32_t> selected_tab_ids = handler_->GetSelectedTabIds();
  EXPECT_THAT(selected_tab_ids, testing::Contains(tab_id));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       SubmitQuery_WaitsForUpload) {
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
      contextual_search::ContextUploadStatus::kProcessing;
  uploading_info.mime_type = lens::MimeType::kPdf;
  uploading_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  EXPECT_CALL(*mock_controller_, GetFileInfo(token))
      .WillRepeatedly(testing::Return(&uploading_info));
  // Do not submit request to server yet.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  handler_->SubmitQuery("Summarize the tab", 0, false, false, false, false,
                        /*is_voice_search=*/false);
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());

  // Now, once file is successfully uploaded, should send request to server.
  uploading_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  SimulateUploadStatusChanged(
      token, lens::MimeType::kPdf,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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

  SimulateUploadStatusChanged(
      *current_token, lens::MimeType::kImage,
      contextual_search::ContextUploadStatus::kProcessing, std::nullopt);
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  SimulateUploadStatusChanged(
      *current_token, lens::MimeType::kImage,
      contextual_search::ContextUploadStatus::kUploadReplaced, std::nullopt);

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
  SimulateUploadStatusChanged(
      *current_token_2, lens::MimeType::kImage,
      contextual_search::ContextUploadStatus::kProcessing, std::nullopt);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  SimulateUploadStatusChanged(
      *current_token_2, lens::MimeType::kImage,
      contextual_search::ContextUploadStatus::kNotUploaded, std::nullopt);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  SimulateUploadStatusChanged(
      *current_token_2, lens::MimeType::kImage,
      contextual_search::ContextUploadStatus::kUploadStarted, std::nullopt);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  SimulateUploadStatusChanged(
      *current_token_2, lens::MimeType::kImage,
      contextual_search::ContextUploadStatus::kProcessingSuggestSignalsReady,
      std::nullopt);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  handler_->SubmitQuery("What is this?", 0, false, false, false, false,
                        /*is_voice_search=*/false);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());

  testing::Mock::VerifyAndClearExpectations(mock_ui_.get());
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  SimulateUploadStatusChanged(
      *current_token_2, lens::MimeType::kImage,
      contextual_search::ContextUploadStatus::kUploadExpired, std::nullopt);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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
      contextual_search::ContextUploadStatus::kProcessing;
  uploading_info.mime_type = lens::MimeType::kPdf;
  uploading_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  EXPECT_CALL(*mock_controller_, GetFileInfo(token))
      .WillRepeatedly(testing::Return(&uploading_info));
  // Do not submit request to server yet.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  // Should stash message instead of submit.
  handler_->SubmitQuery("Summarize the tab", 0, false, false, false, false,
                        /*is_voice_search=*/false);

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

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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
      contextual_search::ContextUploadStatus::kProcessing;
  uploading_info.mime_type = lens::MimeType::kPdf;
  uploading_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  EXPECT_CALL(*mock_controller_, GetFileInfo(token))
      .WillRepeatedly(testing::Return(&uploading_info));
  // Do not submit request to server yet.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  handler_->SubmitQuery("Summarize the tab", 0, false, false, false, false,
                        /*is_voice_search=*/false);
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());

  // Now, once file is deleted, should send request to server.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  handler_->DeleteContext(token, /*from_automatic_chip=*/true);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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
                                kSubmittedContextDecorator),
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
  EXPECT_CALL(
      *mock_controller_,
      StartFileUploadFlow(
          testing::_, testing::A<std::unique_ptr<lens::ContextualInputData>>(),
          testing::_))
      .WillOnce(
          [this](const base::UnguessableToken& file_token,
                 std::unique_ptr<lens::ContextualInputData> data,
                 std::optional<lens::ImageEncodingOptions> image_options) {
            PostUploadStatusChanged(
                file_token, lens::MimeType::kUnknown,
                contextual_search::ContextUploadStatus::kUploadSuccessful);
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
  contextual_search::FileInfo uploading_info{};
  uploading_info.mime_type = lens::MimeType::kPdf;
  uploading_info.upload_status =
      contextual_search::ContextUploadStatus::kProcessing;
  uploading_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());

  EXPECT_CALL(*mock_controller_, GetFileInfo(testing::_))
      .WillRepeatedly(testing::Return(&uploading_info));
  // Should submit when SubmitQuery run + delayed tabs finish uploading.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 1);
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  // No pending query yet since have not submitted yet.
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  handler_->SubmitQuery("What is this?", 0, false, false, false, false,
                        /*is_voice_search=*/false);
  base::RunLoop().RunUntilIdle();

  // Now the delayed tabs should have uploaded.
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);
  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       SubmitQuery_Immediately) {
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
      contextual_search::ContextUploadStatus::kProcessing;
  uploading_info.tab_session_id =
      sessions::SessionTabHelper::IdForTab(active_tab->GetContents());
  EXPECT_CALL(*mock_controller_, GetFileInfo(*current_token))
      .WillRepeatedly(testing::Return(&uploading_info));

  handler_->SubmitQuery("What is this?", 0, false, false, false, false,
                        /*is_voice_search=*/false);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());

  // File is finished uploading.
  uploading_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadSuccessful;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  SimulateUploadStatusChanged(
      *current_token, lens::MimeType::kPdf,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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

  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .WillRepeatedly(
          [](const base::UnguessableToken& file_token,
             std::unique_ptr<lens::ContextualInputData> data,
             std::optional<lens::ImageEncodingOptions> image_options) {
            EXPECT_FALSE(data->is_implicit_upload);
          });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillRepeatedly(testing::Return(lens::ClientToAimMessage()));

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
      contextual_search::ContextUploadStatus::kProcessing;
  info_processing.tab_session_id = session_id;
  EXPECT_CALL(*mock_controller_, GetFileInfo(testing::_))
      .WillRepeatedly(testing::Return(&info_processing));

  // Do not submit to server yet.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  testing::Mock::VerifyAndClearExpectations(mock_controller_.get());
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillRepeatedly(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_controller_, GetFileInfo(testing::_))
      .WillRepeatedly(testing::Return(&info_processing));
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce(
          [this](const base::UnguessableToken& file_token,
                 std::unique_ptr<lens::ContextualInputData> data,
                 std::optional<lens::ImageEncodingOptions> image_options) {
            PostUploadStatusChanged(
                file_token, lens::MimeType::kUnknown,
                contextual_search::ContextUploadStatus::kUploadSuccessful);
          });

  handler_->SubmitQuery("Combined Test", 0, false, false, false, false,
                        /*is_voice_search=*/false);
  base::RunLoop().RunUntilIdle();

  // Delayed tabs should be uploaded once submit is run.
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);

  // Expect that message is stashed instead while normal tab/files are
  // uploading.
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 2);

  // Explicit files and non-delayed tabs need to be manually completed,
  // since they started uploading before the auto-completing mock was set up.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);

  SimulateUploadStatusChanged(
      *normal_tab_token_opt, lens::MimeType::kUnknown,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
  SimulateUploadStatusChanged(
      *file_token_opt, lens::MimeType::kUnknown,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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

  std::vector<base::UnguessableToken> query_tokens;
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .WillRepeatedly(
          [&query_tokens](
              const base::UnguessableToken& file_token,
              std::unique_ptr<lens::ContextualInputData> data,
              std::optional<lens::ImageEncodingOptions> image_options) {
            query_tokens.push_back(file_token);
          });

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillRepeatedly(testing::Return(lens::ClientToAimMessage()));

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

  contextual_search::FileInfo file_info_rB;
  file_info_rB.upload_status =
      contextual_search::ContextUploadStatus::kProcessing;
  file_info_rB.tab_session_id = session_id;
  EXPECT_CALL(*mock_controller_, GetFileInfo(testing::_))
      .WillRepeatedly(testing::Return(&file_info_rB));

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  handler_->SubmitQuery("Stress Test", 0, false, false, false, false,
                        /*is_voice_search=*/false);
  base::RunLoop().RunUntilIdle();

  // Delayed tab #2 finishes uploading.
  ASSERT_TRUE(!delayed_tab_callback.is_null());
  auto data = std::make_unique<lens::ContextualInputData>();
  data->is_page_context_eligible = true;
  std::move(delayed_tab_callback).Run(std::move(data));
  base::RunLoop().RunUntilIdle();

  // Now manually complete the UploadTracker's pending items.
  for (const auto& token : query_tokens) {
    SimulateUploadStatusChanged(
        token, lens::MimeType::kUnknown,
        contextual_search::ContextUploadStatus::kUploadSuccessful);
  }
  base::RunLoop().RunUntilIdle();

  // Verify that still uploading.
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);
  ASSERT_EQ(handler_->GetNumContextUploading(), 1);

  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());

  // Finish uploading file B.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);
  SimulateUploadStatusChanged(
      *token_rB_opt, lens::MimeType::kHtml,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);

  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
  ASSERT_EQ(handler_->GetNumTabsDelayed(), 0);

  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksComposeboxHandlerTest,
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
  base::UnguessableToken file_token;
  EXPECT_CALL(callback, Run(testing::_)).WillOnce([&](const auto& result) {
    ASSERT_TRUE(result.has_value());
    file_token = result.value();
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

  // CreateClientToAimRequest should NOT be called during
  // CreateAndSendQueryMessage because context is uploading.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_)).Times(0);
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);

  EXPECT_TRUE(handler_->HasPendingQueryForTesting());

  // Now complete the upload and verify that CreateClientToAimRequest is called.
  testing::Mock::VerifyAndClearExpectations(mock_controller_.get());
  testing::Mock::VerifyAndClearExpectations(mock_ui_.get());

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&](std::unique_ptr<
                    contextual_search::ContextualSearchContextController::
                        CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->query_text, kQuery);
        EXPECT_THAT(info->file_tokens, testing::Contains(overlay_token));
        EXPECT_TRUE(info->force_include_latest_interaction_request_data);
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);

  SimulateUploadStatusChanged(
      file_token, lens::MimeType::kUnknown,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);

  EXPECT_FALSE(handler_->HasPendingQueryForTesting());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksComposeboxHandlerTest,
    CreateAndSendQueryMessage_WithVisualSelection_AndUploadedTokens) {
  std::string kQuery = "overlay query with tokens";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  base::UnguessableToken overlay_token = base::UnguessableToken::Create();

  // Set task ID.
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Set up uploaded tokens in the session handle used by the handler.
  session_handle_->CreateContextToken();

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

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
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

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       ClearFiles_Delayed) {
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
                                kSubmittedContextDecorator),
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

  // 3. Verify StartFileUploadFlow is NOT called.
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       ClearFiles_BlockAutoSuggestedTabs) {
  GURL url("https://example.com");
  auto create_tab_info = [&]() {
    auto info = std::make_unique<contextual_tasks::SuggestedTabInfo>();
    info->url = url;
    info->title = u"Example";
    return info;
  };

  // 1. Initially, the suggestion should be allowed.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(!received_info.is_null())
            << "Expected a non-null pointer for received_info.";
      });

  auto_suggestion_manager_.SetCurrentSuggestion(create_tab_info());
  handler_->UpdateSuggestedTabContext(
      auto_suggestion_manager_.GetCurrentSuggestion());

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_TRUE(mock_ui_->IsActiveTabContextSuggestionShowing());

  // 2. Blocklist the URL by clearing the files.
  handler_->ClearFiles(/*should_block_auto_suggested_tabs=*/true);

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_FALSE(mock_ui_->IsActiveTabContextSuggestionShowing());

  // 3. Simulate a title change - tab context should still be filtered out.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(received_info.is_null())
            << "Expected a null pointer for received_info.";
      });
  auto_suggestion_manager_.SetCurrentSuggestion(create_tab_info());
  handler_->UpdateSuggestedTabContext(
      auto_suggestion_manager_.GetCurrentSuggestion());

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_FALSE(mock_ui_->IsActiveTabContextSuggestionShowing());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksComposeboxHandlerTestWithAutoSuggestionDisabled,
    UpdateSuggestedTabContext_ForceAllowWhenUploadedViaLens) {
  GURL url("https://example.com");
  AddTab(url);
  auto create_tab_info = [&]() {
    auto info = std::make_unique<contextual_tasks::SuggestedTabInfo>();
    info->url = url;
    info->title = u"Example";
    info->tab_id = TabListInterface::From(browser())
                       ->GetActiveTab()
                       ->GetHandle()
                       .raw_value();
    return info;
  };

  // 2. Set the bool flag on the session handle!
  session_handle_->set_is_contextual_lens_session(true);

  // 3. Expect the suggestion IS ALLOWED despite the feature flag being
  // disabled, because dynamic enabling sees the session bool!
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(!received_info.is_null())
            << "Expected a non-null pointer because session was contextual.";
      });

  auto_suggestion_manager_.SetCurrentSuggestion(create_tab_info());
  handler_->UpdateSuggestedTabContext(
      auto_suggestion_manager_.GetCurrentSuggestion());

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_TRUE(mock_ui_->IsActiveTabContextSuggestionShowing());
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksComposeboxHandlerTestWithAutoSuggestionDisabled,
    UpdateSuggestedTabContext_NoForceAllowWhenVisualQuery) {
  GURL url("https://example.com");
  AddTab(url);
  auto create_tab_info = [&]() {
    auto info = std::make_unique<contextual_tasks::SuggestedTabInfo>();
    info->url = url;
    info->title = u"Example";
    info->tab_id = TabListInterface::From(browser())
                       ->GetActiveTab()
                       ->GetHandle()
                       .raw_value();
    return info;
  };

  // 2. Keep the bool flag on the session handle as false!
  session_handle_->set_is_contextual_lens_session(false);

  // 3. Expect the suggestion IS NOT ALLOWED because it wasn't a text query.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(received_info.is_null())
            << "Expected a null pointer because it was a visual query.";
      });

  auto_suggestion_manager_.SetCurrentSuggestion(create_tab_info());
  handler_->UpdateSuggestedTabContext(
      auto_suggestion_manager_.GetCurrentSuggestion());

  searchbox_page_receiver_.FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       UpdateSuggestedTabContext) {
  GURL url("https://example.com");
  auto create_tab_info = [&]() {
    auto info = std::make_unique<contextual_tasks::SuggestedTabInfo>();
    info->url = url;
    info->title = u"Example";
    return info;
  };

  // 1. Initially, the suggestion should be allowed.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(!received_info.is_null())
            << "Expected a non-null pointer for received_info.";
      });

  auto_suggestion_manager_.SetCurrentSuggestion(create_tab_info());
  handler_->UpdateSuggestedTabContext(
      auto_suggestion_manager_.GetCurrentSuggestion());

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_TRUE(mock_ui_->IsActiveTabContextSuggestionShowing());
  // 2. Blocklist the URL by dismissing an automatic chip.
  // We need to navigate the active tab to the URL being blocklisted.
  AddTab(url);

  handler_->DeleteContext(base::UnguessableToken::Create(),
                          /*from_automatic_chip=*/true);
  // 3. Now the suggestion should be filtered out.
  EXPECT_CALL(mock_searchbox_page_, UpdateAutoSuggestedTabContext(testing::_))
      .WillOnce([&](const searchbox::mojom::TabInfoPtr& received_info) {
        EXPECT_TRUE(received_info.is_null())
            << "Expected a null pointer for received_info.";
      });

  auto_suggestion_manager_.SetCurrentSuggestion(create_tab_info());
  handler_->UpdateSuggestedTabContext(
      auto_suggestion_manager_.GetCurrentSuggestion());

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_FALSE(mock_ui_->IsActiveTabContextSuggestionShowing());
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
  auto_suggestion_manager_.SetCurrentSuggestion(create_tab_info());
  handler_->UpdateSuggestedTabContext(
      auto_suggestion_manager_.GetCurrentSuggestion());

  searchbox_page_receiver_.FlushForTesting();
  EXPECT_TRUE(mock_ui_->IsActiveTabContextSuggestionShowing());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       AddFileContext_NullSessionHandle) {
  // Create a handler with a callback that returns nullptr for session handle.
  mojo::PendingRemote<composebox::mojom::Page> page_remote;
  mojo::PendingReceiver<composebox::mojom::Page> page_receiver =
      page_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<searchbox::mojom::Page> searchbox_page_remote;
  mojo::PendingReceiver<searchbox::mojom::Page> searchbox_page_receiver =
      searchbox_page_remote.InitWithNewPipeAndPassReceiver();

  auto handler = std::make_unique<TestContextualTasksComposeboxHandler>(
      mock_ui_.get(), profile(), web_contents(),
      mojo::PendingReceiver<composebox::mojom::PageHandler>(),
      std::move(page_remote),
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      std::move(searchbox_page_remote),
      base::BindRepeating(
          []() -> contextual_search::ContextualSearchSessionHandle* {
            return nullptr;
          }),
      base::DoNothing(),
      base::BindRepeating(&ContextualTasksUI::TakeInputStateModel,
                          base::Unretained(mock_ui_.get())));
  auto file_info = searchbox::mojom::SelectedFileInfo::New();
  std::vector<uint8_t> data = {0x1};
  mojo_base::BigBuffer file_bytes(data);

  base::MockCallback<ContextualTasksComposeboxHandler::AddFileContextCallback>
      callback;
  base::expected<base::UnguessableToken,
                 contextual_search::ContextUploadErrorType>
      result;

  EXPECT_CALL(callback, Run(testing::_)).WillOnce(testing::SaveArg<0>(&result));

  handler->AddFileContext(std::move(file_info), std::move(file_bytes),
                          callback.Get());

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(),
            contextual_search::ContextUploadErrorType::kBrowserProcessingError);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       OnContextUploadStatusChanged_LensOverlayToken_Ignored) {
  base::UnguessableToken lens_token = base::UnguessableToken::Create();

  EXPECT_CALL(*mock_lens_controller_->mock_router(),
              overlay_tab_context_file_token())
      .WillRepeatedly(testing::Return(lens_token));
  EXPECT_CALL(mock_searchbox_page_, OnContextualInputStatusChanged(
                                        testing::_, testing::_, testing::_))
      .Times(0);

  handler_->OnContextUploadStatusChanged(
      lens_token, lens::MimeType::kUnknown,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);

  // Verify that for a different token, it IS called.
  base::UnguessableToken other_token = base::UnguessableToken::Create();
  EXPECT_CALL(mock_searchbox_page_, OnContextualInputStatusChanged(
                                        testing::_, testing::_, testing::_))
      .Times(1);

  handler_->OnContextUploadStatusChanged(
      other_token, lens::MimeType::kUnknown,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       ActiveModelIsPassed) {
  // 1. Arrange: Setup a mock callback to simulate ContextualTasksUI returning a
  // model. We explicitly set a distinct state (MODEL_MODE_GEMINI_PRO) to verify
  // it gets passed correctly.
  auto mock_callback = base::BindRepeating(
      &ContextualTasksComposeboxHandlerTest::CreateMockInputStateModel,
      base::Unretained(this));
  mojo::PendingRemote<composebox::mojom::Page> page_remote;
  mojo::PendingReceiver<composebox::mojom::Page> page_receiver =
      page_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<searchbox::mojom::Page> searchbox_page_remote;
  mojo::PendingReceiver<searchbox::mojom::Page> searchbox_page_receiver =
      searchbox_page_remote.InitWithNewPipeAndPassReceiver();
  auto custom_handler = std::make_unique<TestContextualTasksComposeboxHandler>(
      mock_ui_.get(), profile(), web_contents(),
      mojo::PendingReceiver<composebox::mojom::PageHandler>(),
      std::move(page_remote),
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      std::move(searchbox_page_remote),
      base::BindRepeating(
          &ContextualTasksUI::GetOrCreateContextualSessionHandle,
          base::Unretained(mock_ui_.get())),
      base::BindRepeating(&ContextualTasksUI::ClearContextualSessionHandle,
                          base::Unretained(mock_ui_.get())),
      std::move(mock_callback));

  // 2. Act: Trigger the handler to fetch the model via the callback.
  custom_handler->InitializeInputStateModel();

  // 3. Assert: Verify the handler successfully took ownership of the model
  // and the internal state matches exactly what the callback provided.
  contextual_search::InputStateModel* handler_model =
      custom_handler->TakeInputStateModelForTesting();

  ASSERT_NE(handler_model, nullptr);
  EXPECT_EQ(handler_model->get_state_for_testing().active_model,
            omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       SuggestInputsCallbackWorks) {
  auto mock_session =
      std::make_unique<contextual_search::MockContextualSearchSessionHandle>();

  lens::proto::LensOverlaySuggestInputs suggest_inputs;

  EXPECT_CALL(*mock_session, GetSuggestInputs())
      .WillRepeatedly(testing::Return(suggest_inputs));

  auto mock_session_ptr = mock_session.get();

  auto mock_get_session_callback = base::BindRepeating(
      [](contextual_search::MockContextualSearchSessionHandle* ptr)
          -> contextual_search::ContextualSearchSessionHandle* { return ptr; },
      mock_session_ptr);

  mojo::PendingRemote<composebox::mojom::Page> page_remote;
  mojo::PendingReceiver<composebox::mojom::Page> page_receiver =
      page_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<searchbox::mojom::Page> searchbox_page_remote;
  mojo::PendingReceiver<searchbox::mojom::Page> searchbox_page_receiver =
      searchbox_page_remote.InitWithNewPipeAndPassReceiver();

  auto custom_handler = std::make_unique<TestContextualTasksComposeboxHandler>(
      mock_ui_.get(), profile(), web_contents(),
      mojo::PendingReceiver<composebox::mojom::PageHandler>(),
      std::move(page_remote),
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      std::move(searchbox_page_remote), mock_get_session_callback,
      base::BindRepeating(&ContextualTasksUI::ClearContextualSessionHandle,
                          base::Unretained(mock_ui_.get())),
      base::BindRepeating(&ContextualTasksUI::TakeInputStateModel,
                          base::Unretained(mock_ui_.get())));

  auto* client = static_cast<ContextualOmniboxClient*>(
      custom_handler->GetOmniboxControllerForTesting()->client());

  auto result = client->GetLensOverlaySuggestInputsForTesting();

  ASSERT_TRUE(result.has_value());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       OnLensThumbnailCreated_TriggersUploadStatusChanges) {
  // Setup: mock the overlay token.
  base::UnguessableToken overlay_token = base::UnguessableToken::Create();
  EXPECT_CALL(*mock_lens_controller_->mock_router(),
              overlay_tab_context_file_token())
      .WillRepeatedly(testing::Return(overlay_token));

  // Mock OnContextUploadStatusChanged to verify calls and forward to real
  // implementation.
  // We capture the uploaded tokens to verify them.
  std::vector<base::UnguessableToken> successful_uploads;
  std::vector<base::UnguessableToken> replaced_uploads;

  EXPECT_CALL(*handler_, OnContextUploadStatusChanged(testing::_, testing::_,
                                                      testing::_, testing::_))
      .WillRepeatedly([&](const base::UnguessableToken& context_token,
                          lens::MimeType mime_type,
                          contextual_search::ContextUploadStatus
                              context_upload_status,
                          const std::optional<
                              contextual_search::ContextUploadErrorType>&
                              error_type) {
        if (context_upload_status ==
            contextual_search::ContextUploadStatus::kUploadSuccessful) {
          successful_uploads.push_back(context_token);
        } else if (context_upload_status ==
                   contextual_search::ContextUploadStatus::kUploadReplaced) {
          replaced_uploads.push_back(context_token);
        }
        handler_
            ->ContextualTasksComposeboxHandler::OnContextUploadStatusChanged(
                context_token, mime_type, context_upload_status, error_type);
      });

  // 1. First selection.
  std::string thumbnail_data = "data:image/png;base64,DATA";
  handler_->OnLensThumbnailCreated(thumbnail_data);

  // Verify: Should have one successful upload and no replacements.
  ASSERT_EQ(successful_uploads.size(), 1u);
  ASSERT_EQ(replaced_uploads.size(), 0u);
  base::UnguessableToken first_token = successful_uploads[0];

  // 2. Second selection (replace).
  std::string thumbnail_data_2 = "data:image/png;base64,DATA2";
  handler_->OnLensThumbnailCreated(thumbnail_data_2);

  // Verify: Should have one replacement (the first token) and one new
  // successful upload.
  ASSERT_EQ(successful_uploads.size(), 2u);
  ASSERT_EQ(replaced_uploads.size(), 1u);
  EXPECT_EQ(replaced_uploads[0], first_token);
  EXPECT_NE(successful_uploads[1], first_token);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       SubmitQuery_WaitsForRecontextualization) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "stashed query";
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
                                kSubmittedContextDecorator),
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
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadExpired;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);
  file_info_list.push_back(&file_info);

  EXPECT_CALL(*mock_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  // 1. Capture the GetPageContext callback.
  MockTabContextualizationController::GetPageContextCallback pending_callback;
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillOnce([&](MockTabContextualizationController::GetPageContextCallback
                        callback) { pending_callback = std::move(callback); });

  // 2. Call CreateAndSendQueryMessage.
  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);

  // Verify: recontextualization is pending, so the query is blocked.
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  ASSERT_FALSE(pending_callback.is_null());

  // 3. Start file upload when recontextualizer completes context fetch.
  base::UnguessableToken uploaded_token;
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .WillOnce([&](const base::UnguessableToken& file_token,
                    std::unique_ptr<lens::ContextualInputData> data,
                    std::optional<lens::ImageEncodingOptions> image_options) {
        EXPECT_TRUE(data->is_implicit_upload);
        uploaded_token = file_token;
      });

  auto data = std::make_unique<lens::ContextualInputData>();
  data->tab_session_id = session_id;
  data->page_url = GURL("about:blank");
  data->page_title = "about:blank";
  data->context_id = 12345;
  data->is_page_context_eligible = true;
  std::move(pending_callback).Run(std::move(data));

  // Verify: Still uploading and query not stashed in handler (as
  // recontextualization waits in UploadTracker before
  // ContinueCreateAndSendQueryMessage is triggered).
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());

  // 4. Complete the upload status change.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  PostUploadStatusChanged(
      uploaded_token, lens::MimeType::kUnknown,
      contextual_search::ContextUploadStatus::kUploadSuccessful);
  run_loop.Run();

  // Verify: Upload finished, stashed query is successfully sent.
  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       Recontextualization_TabInvalidatedGracefullyCompletes) {
  ASSERT_NE(mock_contextual_tasks_service_ptr_, nullptr)
      << "Mock controller is NULL!";
  std::string kQuery = "invalid tab query";
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context.
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
                                kSubmittedContextDecorator),
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
  file_info.upload_status =
      contextual_search::ContextUploadStatus::kUploadExpired;
  file_info.request_id.emplace();
  file_info.request_id->set_context_id(12345);
  file_info_list.push_back(&file_info);

  EXPECT_CALL(*mock_controller_, GetFileInfoList())
      .WillRepeatedly(testing::Return(file_info_list));

  // 1. GetPageContext returns nullptr, representing tab was invalidated.
  EXPECT_CALL(*mock_tab_controller_, GetPageContext(testing::_))
      .WillOnce([](MockTabContextualizationController::GetPageContextCallback
                       callback) { std::move(callback).Run(nullptr); });

  // 2. Upload is NOT triggered.
  EXPECT_CALL(*mock_controller_,
              StartFileUploadFlow(testing::_, testing::_, testing::_))
      .Times(0);

  // 3. Complete direct AIM query submission without context upload.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  base::RunLoop run_loop;
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));

  handler_->CreateAndSendQueryMessage(kQuery, /*is_voice_search=*/false);
  run_loop.Run();

  // Verify: No context was uploaded, pending uploads are back to 0.
  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       SubmitQuery_WaitsForModalityChipUpload) {
  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  EXPECT_CALL(*mock_ui_, GetTaskId())
      .WillRepeatedly(
          testing::ReturnRefOfCopy(std::optional<base::Uuid>(task_id)));

  // Setup context.
  contextual_tasks::ContextualTask task(task_id);
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

  base::UnguessableToken token = base::UnguessableToken::Create();

  // Setup FileInfo representing a server-injected modality chip in kProcessing
  // status.
  contextual_search::FileInfo uploading_info{};
  uploading_info.upload_status =
      contextual_search::ContextUploadStatus::kProcessing;
  uploading_info.mime_type = lens::MimeType::kUnknown;
  auto input_data = std::make_unique<lens::ContextualInputData>();
  input_data->modality_chip_props.emplace();
  input_data->modality_chip_props->set_id("test_chip_id");
  uploading_info.input_data = std::move(input_data);

  EXPECT_CALL(*mock_controller_, GetFileInfo(token))
      .WillRepeatedly(testing::Return(&uploading_info));

  // Expect no queries are sent immediately because the chip is still uploading.
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

  // Simulate the status transition to kProcessing, which should register the
  // modality chip in the handler's pending uploads set.
  SimulateUploadStatusChanged(
      token, lens::MimeType::kUnknown,
      contextual_search::ContextUploadStatus::kProcessing, std::nullopt);

  // Verify the chip is tracked as uploading.
  ASSERT_TRUE(handler_->IsAnyContextUploading());
  ASSERT_EQ(handler_->GetNumContextUploading(), 1);

  // Submit query manually. It should be stashed.
  handler_->SubmitQuery("Test query", 0, false, false, false, false,
                        /*is_voice_search=*/false);
  ASSERT_TRUE(handler_->HasPendingQueryForTesting());

  // Now expect the stashed query to be sent when the chip completes
  // successfully.
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(1);

  // Simulate transition to kUploadSuccessful.
  SimulateUploadStatusChanged(
      token, lens::MimeType::kUnknown,
      contextual_search::ContextUploadStatus::kUploadSuccessful, std::nullopt);

  // Verify pending uploads are cleared and the query has been sent.
  ASSERT_FALSE(handler_->IsAnyContextUploading());
  ASSERT_FALSE(handler_->HasPendingQueryForTesting());
  ASSERT_EQ(handler_->GetNumContextUploading(), 0);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       MultiFilesSelected) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath file1_path = temp_dir.GetPath().AppendASCII("file1.pdf");
  base::FilePath file2_path = temp_dir.GetPath().AppendASCII("file2.png");

  std::string file1_content = "dummy pdf content";
  std::string file2_content = "dummy image content";

  ASSERT_TRUE(base::WriteFile(file1_path, file1_content));
  ASSERT_TRUE(base::WriteFile(file2_path, file2_content));

  std::vector<ui::SelectedFileInfo> files;
  files.emplace_back(file1_path, file1_path);
  files.emplace_back(file2_path, file2_path);

  base::RunLoop run_loop;
  int file_contexts_added = 0;
  EXPECT_CALL(mock_searchbox_page_, AddFileContext(testing::_, testing::_))
      .Times(2)
      .WillRepeatedly([&](const base::UnguessableToken& token,
                          searchbox::mojom::SelectedFileInfoPtr file_info) {
        file_contexts_added++;
        if (file_info->file_name == "file1.pdf") {
          EXPECT_EQ(file_info->mime_type, "application/pdf");
        } else if (file_info->file_name == "file2.png") {
          EXPECT_EQ(file_info->mime_type, "image/png");
        }
        if (file_contexts_added == 2) {
          run_loop.Quit();
        }
      });

  handler_->MultiFilesSelected(files);

  // Wait for ThreadPool tasks to finish processing files.
  run_loop.Run();

  EXPECT_EQ(handler_->GetNumContextUploading(), 2);
}

IN_PROC_BROWSER_TEST_F(ContextualTasksComposeboxHandlerTest,
                       UpdateStateFromUrl_SoftNavigation) {
  // Arrange: Setup local config with Canvas tool and its url params.
  omnibox::SearchboxConfig config;
  auto* canvas_config = config.add_tool_configs();
  canvas_config->set_tool(omnibox::ToolMode::TOOL_MODE_CANVAS);
  auto* canvas_param = canvas_config->add_aim_url_params();
  canvas_param->set_param_key("rc");
  canvas_param->set_param_value("1");

  // Create composebox handler using a custom callback that binds the local
  // config.
  auto mock_callback = base::BindRepeating(
      [](contextual_search::ContextualSearchSessionHandle* session_handle,
         const omnibox::SearchboxConfig config) {
        return std::make_unique<contextual_search::InputStateModel>(
            *session_handle, config, GURL(), /*is_off_the_record=*/false,
            /*browser_identity_matches_aim_identity=*/false);
      },
      session_handle_.get(), config);

  mojo::PendingRemote<composebox::mojom::Page> page_remote;
  mojo::PendingReceiver<composebox::mojom::Page> page_receiver =
      page_remote.InitWithNewPipeAndPassReceiver();
  mojo::PendingRemote<searchbox::mojom::Page> searchbox_page_remote;
  mojo::PendingReceiver<searchbox::mojom::Page> searchbox_page_receiver =
      searchbox_page_remote.InitWithNewPipeAndPassReceiver();
  auto custom_handler = std::make_unique<TestContextualTasksComposeboxHandler>(
      mock_ui_.get(), profile(), web_contents(),
      mojo::PendingReceiver<composebox::mojom::PageHandler>(),
      std::move(page_remote),
      mojo::PendingReceiver<searchbox::mojom::PageHandler>(),
      std::move(searchbox_page_remote),
      base::BindRepeating(
          &ContextualTasksUI::GetOrCreateContextualSessionHandle,
          base::Unretained(mock_ui_.get())),
      base::BindRepeating(&ContextualTasksUI::ClearContextualSessionHandle,
                          base::Unretained(mock_ui_.get())),
      std::move(mock_callback));

  custom_handler->InitializeInputStateModel();

  contextual_search::InputStateModel* model =
      custom_handler->TakeInputStateModelForTesting();
  ASSERT_NE(model, nullptr);

  // Default tool is unspecified.
  EXPECT_EQ(model->get_state_for_testing().active_tool,
            omnibox::ToolMode::TOOL_MODE_UNSPECIFIED);

  // Act: Simulate soft navigation by calling UpdateStateFromUrl with Canvas
  // GURL.
  GURL canvas_url("https://example.com/?rc=1");
  custom_handler->UpdateStateFromUrl(canvas_url);

  // Assert: Verify the tool successfully restored/persisted to Canvas.
  EXPECT_EQ(model->get_state_for_testing().active_tool,
            omnibox::ToolMode::TOOL_MODE_CANVAS);
  EXPECT_TRUE(model->get_state_for_testing().is_canvas_query_submitted);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksComposeboxHandlerTestWithContextManagementEnabled,
    SetAimThreadRestoredTabs) {
  SetUpHandler();
  ASSERT_NE(handler_, nullptr);

  std::vector<searchbox::mojom::TabInfoPtr> restored_tabs;
  auto tab_info = searchbox::mojom::TabInfo::New();
  tab_info->url = GURL("https://example.com");
  tab_info->title = "Example Site";
  restored_tabs.push_back(std::move(tab_info));

  EXPECT_CALL(mock_searchbox_page_, SetAimThreadRestoredTabs(testing::_))
      .WillOnce([&](std::vector<searchbox::mojom::TabInfoPtr> tabs) {
        EXPECT_EQ(tabs.size(), 1u);
        EXPECT_EQ(tabs[0]->url, GURL("https://example.com"));
        EXPECT_EQ(tabs[0]->title, "Example Site");
      });

  handler_->SetAimThreadRestoredTabs(std::move(restored_tabs));
}
