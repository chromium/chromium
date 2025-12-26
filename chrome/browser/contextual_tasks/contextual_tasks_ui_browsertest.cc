// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_factory.h"
#include "chrome/browser/contextual_tasks/mock_contextual_tasks_context_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"

namespace {

const char kTestEmail[] = "test@example.com";
const char kTestToken[] = "test_token";

using testing::_;
using testing::Invoke;

// Mock the Page interface to verify calls from C++ to the Renderer (JS).
class MockContextualTasksPage : public contextual_tasks::mojom::Page {
 public:
  MockContextualTasksPage() = default;
  ~MockContextualTasksPage() override = default;

  MOCK_METHOD(void, SetOAuthToken, (const std::string& token), (override));
  MOCK_METHOD(void, SetThreadTitle, (const std::string& title), (override));
  MOCK_METHOD(void, OnSidePanelStateChanged, (), (override));
  MOCK_METHOD(void,
              PostMessageToWebview,
              (const std::vector<uint8_t>& message),
              (override));
  MOCK_METHOD(void, OnHandshakeComplete, (), (override));
  MOCK_METHOD(void,
              OnContextUpdated,
              (std::vector<contextual_tasks::mojom::TabPtr>),
              (override));
  MOCK_METHOD(void, HideInput, (), (override));
  MOCK_METHOD(void, RestoreInput, (), (override));
  MOCK_METHOD(void, OnZeroStateChange, (bool is_zero_state), (override));
  MOCK_METHOD(void, OnAiPageStatusChanged, (bool), (override));
  MOCK_METHOD(void,
              SetTaskDetails,
              (const base::Uuid&, const std::string&, const std::string&),
              (override));

  mojo::PendingRemote<contextual_tasks::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<contextual_tasks::mojom::Page> receiver_{this};
};

}  // namespace

class ContextualTasksUIBrowserTest : public InProcessBrowserTest {
 public:
  ContextualTasksUIBrowserTest() = default;
  ~ContextualTasksUIBrowserTest() override = default;

  // This override is required to inject the FakeProfileOAuth2TokenService
  // factory BEFORE the Browser (and its Profile) are created. This is a
  // requirement for IdentityTestEnvironment to work.
  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&ContextualTasksUIBrowserTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  // This callback installs the fake factories for IdentityTestEnvironment.
  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    IdentityTestEnvironmentProfileAdaptor::
        SetIdentityTestEnvironmentFactoriesOnBrowserContext(context);
    contextual_tasks::ContextualTasksContextControllerFactory::GetInstance()
        ->SetTestingFactory(
            context, base::BindRepeating([](content::BrowserContext*)
                                             -> std::unique_ptr<KeyedService> {
              return std::make_unique<testing::NiceMock<
                  contextual_tasks::MockContextualTasksContextController>>();
            }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    // Sign in the user so IdentityManager is ready.
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    identity_test_env_ =
        identity_test_environment_adaptor_->identity_test_env();
    identity_test_env_->MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);

    // Setup TestWebUI.
    auto* web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    test_web_ui_ = std::make_unique<content::TestWebUI>();
    test_web_ui_->set_web_contents(web_contents);

    // Create the ContextualTasksUI which will be used for testing.
    controller_ = std::make_unique<ContextualTasksUI>(test_web_ui_.get());
  }

  void TearDownOnMainThread() override {
    controller_.reset();
    test_web_ui_.reset();
    identity_test_env_ = nullptr;
    identity_test_environment_adaptor_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<ContextualTasksUI> controller_;
  base::CallbackListSubscription create_services_subscription_;
};

// Verify that the OAuth token is requested and sent when the page handler is
// created.
IN_PROC_BROWSER_TEST_F(ContextualTasksUIBrowserTest, RequestOAuthTokenManual) {
  testing::NiceMock<MockContextualTasksPage> mock_page;
  base::RunLoop run_loop;

  // Verify that the token is set in the mocked page.
  EXPECT_CALL(mock_page, SetOAuthToken(_))
      .WillOnce([&run_loop](const std::string& token) {
        EXPECT_EQ(kTestToken, token);
        run_loop.Quit();
      });

  // Create the PageHandler to mimic the Mojo call from the renderer, but allows
  // the test to mock remote directly.
  mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> handler_receiver;
  controller_->CreatePageHandler(mock_page.BindAndGetRemote(),
                                 std::move(handler_receiver));

  //  Wait for the token request to be made.
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kTestToken, base::Time::Now() + base::Days(10));

  // Verify that the token is set in the mocked page.
  run_loop.Run();
}

// Verify that the OAuth token is refreshed after it expires.
IN_PROC_BROWSER_TEST_F(ContextualTasksUIBrowserTest,
                       RequestOAuthTokenRefreshes) {
  testing::NiceMock<MockContextualTasksPage> mock_page;
  base::RunLoop run_loop;

  // Expect SetOAuthToken to be called twice.
  EXPECT_CALL(mock_page, SetOAuthToken(_))
      .WillOnce([&](const std::string& token) {
        EXPECT_EQ(kTestToken, token);
        // Enable auto-issue for the next request. This will cause the token
        // next token to be issued as "account_token".
        identity_test_env_->SetAutomaticIssueOfAccessTokens(true);
      })
      .WillOnce([&](const std::string& token) {
        // Verify that the token is refreshed. It should be different from the
        // first token.
        EXPECT_NE(kTestToken, token);
        run_loop.Quit();
      });

  mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> handler_receiver;
  controller_->CreatePageHandler(mock_page.BindAndGetRemote(),
                                 std::move(handler_receiver));

  // Respond to the first request with a short expiration.
  base::Time expiration = base::Time::Now() + base::Seconds(1);
  identity_test_env_->WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      kTestToken, expiration);

  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUIBrowserTest,
                       OnSidePanelStateChanged_InTab) {
  testing::NiceMock<MockContextualTasksPage> mock_page;

  mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> handler_receiver;
  controller_->CreatePageHandler(mock_page.BindAndGetRemote(),
                                 std::move(handler_receiver));

  base::RunLoop run_loop;
  // Expect OnSidePanelStateChanged to be called on the page.
  EXPECT_CALL(mock_page, OnSidePanelStateChanged()).Times(1);

  // Expect PostMessageToWebview to be called with the correct display mode.
  EXPECT_CALL(mock_page, PostMessageToWebview(_))
      .WillOnce([&run_loop](const std::vector<uint8_t>& message) {
        lens::ClientToAimMessage client_message;
        ASSERT_TRUE(
            client_message.ParseFromArray(message.data(), message.size()));
        ASSERT_TRUE(client_message.has_set_cobrowsing_display_mode());
        EXPECT_EQ(client_message.set_cobrowsing_display_mode()
                      .payload()
                      .display_mode(),
                  lens::CobrowsingDisplayModeParams::COBROWSING_TAB);
        run_loop.Quit();
      });

  controller_->OnSidePanelStateChanged();
  run_loop.Run();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUIBrowserTest,
                       OnSidePanelStateChanged_InSidePanel) {
  // Create a WebContents not associated with a tab.
  std::unique_ptr<content::WebContents> side_panel_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  auto side_panel_web_ui = std::make_unique<content::TestWebUI>();
  side_panel_web_ui->set_web_contents(side_panel_contents.get());

  auto side_panel_controller =
      std::make_unique<ContextualTasksUI>(side_panel_web_ui.get());

  testing::NiceMock<MockContextualTasksPage> mock_page;
  mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> handler_receiver;
  side_panel_controller->CreatePageHandler(mock_page.BindAndGetRemote(),
                                           std::move(handler_receiver));

  base::RunLoop run_loop;
  EXPECT_CALL(mock_page, OnSidePanelStateChanged()).Times(1);
  EXPECT_CALL(mock_page, PostMessageToWebview(_))
      .WillOnce([&run_loop](const std::vector<uint8_t>& message) {
        lens::ClientToAimMessage client_message;
        ASSERT_TRUE(
            client_message.ParseFromArray(message.data(), message.size()));
        ASSERT_TRUE(client_message.has_set_cobrowsing_display_mode());
        EXPECT_EQ(client_message.set_cobrowsing_display_mode()
                      .payload()
                      .display_mode(),
                  lens::CobrowsingDisplayModeParams::COBROWSING_SIDEPANEL);
        run_loop.Quit();
      });

  side_panel_controller->OnSidePanelStateChanged();
  run_loop.Run();
}
