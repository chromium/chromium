// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"

#include <memory>
#include <utility>

#include "base/memory/raw_ptr.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_context_controller.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/webui/searchbox/searchbox_test_utils.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_search/contextual_search_metrics_recorder.h"
#include "components/contextual_search/contextual_search_service.h"
#include "components/contextual_search/contextual_search_session_handle.h"
#include "components/contextual_search/fake_variations_client.h"
#include "components/contextual_search/mock_contextual_search_context_controller.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/variations/scoped_variations_ids_provider.h"
#include "components/variations/variations_switches.h"
#include "content/public/browser/browser_context.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/test_web_contents_factory.h"
#include "content/public/test/test_web_ui.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "url/gurl.h"

class BrowserWindowInterface;
class TemplateURLService;

class LocalContextualSearchboxHandlerTestHarness : public testing::Test {
 public:
  LocalContextualSearchboxHandlerTestHarness()
      : test_shared_loader_factory_(
            base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
                &url_loader_factory_)),
        get_variations_ids_provider_(
            variations::VariationsIdsProvider::Mode::kUseSignedInState) {
    web_contents_ = web_contents_factory_.CreateWebContents(&profile_);
  }
  ~LocalContextualSearchboxHandlerTestHarness() override = default;

 protected:
  content::BrowserTaskEnvironment task_environment_;
  TestingProfile profile_;
  network::TestURLLoaderFactory url_loader_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> test_shared_loader_factory_;
  content::TestWebContentsFactory web_contents_factory_;

  raw_ptr<content::WebContents> web_contents_;
  raw_ptr<TemplateURLService> template_url_service_ = nullptr;
  contextual_search::FakeVariationsClient fake_variations_client_;
  variations::test::ScopedVariationsIdsProvider get_variations_ids_provider_;

  // Helper methods to access protected members
  content::WebContents* web_contents() { return web_contents_; }
  TestingProfile* profile() { return &profile_; }
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
};

class MockContextualTasksUiService
    : public contextual_tasks::ContextualTasksUiService {
 public:
  MockContextualTasksUiService(
      Profile* profile,
      contextual_tasks::ContextualTasksContextController* context_controller)
      : contextual_tasks::ContextualTasksUiService(profile,
                                                   context_controller,
                                                   nullptr) {}
  ~MockContextualTasksUiService() override = default;

  MOCK_METHOD(void,
              OnTaskChangedInPanel,
              (BrowserWindowInterface * browser_window_interface,
               content::WebContents* web_contents,
               const base::Uuid& task_id),
              (override));
};

class ContextualTasksComposeboxHandlerTest
    : public LocalContextualSearchboxHandlerTestHarness {
 public:
  ContextualTasksComposeboxHandlerTest() = default;
  ~ContextualTasksComposeboxHandlerTest() override = default;

  void SetUp() override {
    LocalContextualSearchboxHandlerTestHarness::SetUp();
    web_ui_.set_web_contents(web_contents());

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
            contextual_search::ContextualSearchSource::kLens));
    session_handle_ =
        service_->GetSession(contextual_session_handle->session_id());
    ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents())
        ->set_session_handle(std::move(contextual_session_handle));

    mock_ui_ =
        std::make_unique<testing::NiceMock<MockContextualTasksUI>>(&web_ui_);
    ON_CALL(*mock_ui_, GetWebUIWebContents())
        .WillByDefault(testing::Return(web_contents()));

    handler_ = std::make_unique<ContextualTasksComposeboxHandler>(
        mock_ui_.get(), profile(), web_contents(),
        mojo::PendingReceiver<composebox::mojom::PageHandler>(),
        mojo::PendingRemote<composebox::mojom::Page>(),
        mojo::PendingReceiver<searchbox::mojom::PageHandler>());
  }

  void TearDown() override {
    handler_.reset();
    mock_controller_ = nullptr;
    session_handle_.reset();
    service_.reset();
    mock_ui_.reset();
    LocalContextualSearchboxHandlerTestHarness::TearDown();
  }

 protected:
  content::TestWebUI web_ui_;
  std::unique_ptr<testing::NiceMock<MockContextualTasksUI>> mock_ui_;
  std::unique_ptr<ContextualTasksComposeboxHandler> handler_;

  // For session management.
  std::unique_ptr<contextual_search::ContextualSearchService> service_;
  std::unique_ptr<contextual_search::ContextualSearchSessionHandle>
      session_handle_;
  raw_ptr<contextual_search::MockContextualSearchContextController>
      mock_controller_;
};

TEST_F(ContextualTasksComposeboxHandlerTest, SubmitQuery) {
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce(testing::Return(lens::ClientToAimMessage()));
  EXPECT_CALL(*mock_ui_, GetWebUIWebContents())
      .WillOnce(testing::Return(web_contents()));
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));
  handler_->SubmitQuery("test query", 0, false, false, false, false);
}

TEST_F(ContextualTasksComposeboxHandlerTest, SubmitQuery_NoSession) {
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents())
      ->set_session_handle(nullptr);

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);
  handler_->SubmitQuery("test query", 0, false, false, false, false);
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

TEST_F(ContextualTasksComposeboxHandlerTest, OnAutocompleteAccept_NoSession) {
  ContextualSearchWebContentsHelper::GetOrCreateForWebContents(web_contents())
      ->set_session_handle(nullptr);

  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_)).Times(0);

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

TEST_F(ContextualTasksComposeboxHandlerTest, CreateAndSendQueryMessage) {
  std::string kQuery = "direct query";
  EXPECT_CALL(*mock_controller_, CreateClientToAimRequest(testing::_))
      .WillOnce([&kQuery](std::unique_ptr<
                          contextual_search::ContextualSearchContextController::
                              CreateClientToAimRequestInfo> info) {
        EXPECT_EQ(info->query_text, kQuery);
        EXPECT_EQ(info->query_text_source,
                  lens::QueryPayload::QUERY_TEXT_SOURCE_KEYBOARD_INPUT);
        return lens::ClientToAimMessage();
      });
  EXPECT_CALL(*mock_ui_, PostMessageToWebview(testing::_));

  handler_->CreateAndSendQueryMessage(kQuery);
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
