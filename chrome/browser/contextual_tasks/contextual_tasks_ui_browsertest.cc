// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/tab_list/tab_list_interface.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/lens/lens_search_controller.h"
#include "chrome/browser/ui/tabs/public/tab_features.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/lens/lens_overlay_invocation_source.h"
#include "components/omnibox/browser/searchbox.mojom.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/tabs/public/tab_interface.h"
#include "components/zoom/zoom_controller.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_web_ui.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/lens_server_proto/aim_communication.pb.h"
#include "ui/webui/resources/cr_components/composebox/composebox.mojom.h"

namespace {

const char kTestEmail[] = "test@example.com";

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
              (std::vector<contextual_tasks::mojom::ContextInfoPtr>),
              (override));
  MOCK_METHOD(void, HideInput, (), (override));
  MOCK_METHOD(void, RestoreInput, (), (override));
  MOCK_METHOD(void, OnZeroStateChange, (bool is_zero_state), (override));
  MOCK_METHOD(void, OnAiPageStatusChanged, (bool), (override));
  MOCK_METHOD(void,
              OnLensOverlayStateChanged,
              (bool is_showing, bool maybe_show_overlay_hint_text),
              (override));
  MOCK_METHOD(void,
              SetTaskDetails,
              (const base::Uuid&, const std::string&, const std::string&),
              (override));
  MOCK_METHOD(void, SetAimUrl, (const GURL&), (override));
  MOCK_METHOD(void, ShowErrorPage, (), (override));
  MOCK_METHOD(void, HideErrorPage, (), (override));
  MOCK_METHOD(void, ShowOauthErrorDialog, (), (override));
  MOCK_METHOD(void,
              UpdateComposeboxPosition,
              (contextual_tasks::mojom::ComposeboxPositionPtr),
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

  mojo::PendingRemote<contextual_tasks::mojom::Page> BindAndGetRemote() {
    return receiver_.BindNewPipeAndPassRemote();
  }

 private:
  mojo::Receiver<contextual_tasks::mojom::Page> receiver_{this};
};

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
};

class MockContextualTasksCookieSynchronizer
    : public contextual_tasks::ContextualTasksCookieSynchronizer {
 public:
  MockContextualTasksCookieSynchronizer(
      content::BrowserContext* context,
      signin::IdentityManager* identity_manager)
      : ContextualTasksCookieSynchronizer(context, identity_manager) {}
  ~MockContextualTasksCookieSynchronizer() override = default;

  MOCK_METHOD(void, CopyCookiesToWebviewStoragePartition, (), (override));
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
    contextual_tasks::ContextualTasksServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindRepeating(
                [](content::BrowserContext*) -> std::unique_ptr<KeyedService> {
                  return std::make_unique<testing::NiceMock<
                      contextual_tasks::MockContextualTasksService>>();
                }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    // Sign in the user so IdentityManager is ready.
    identity_test_environment_adaptor_ =
        std::make_unique<IdentityTestEnvironmentProfileAdaptor>(
            browser()->profile());
    identity_test_env_ =
        identity_test_environment_adaptor_->identity_test_env();
    identity_test_env_->MakePrimaryAccountAvailable(
        kTestEmail, signin::ConsentLevel::kSignin);

    // Setup TestWebUI.
    content::WebContents* web_contents =
        TabListInterface::From(browser())->GetActiveTab()->GetContents();
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

  void TriggerOnInnerWebContentsCreated(content::WebContents* inner) {
    controller_->OnInnerWebContentsCreated(inner);
  }

 protected:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<ContextualTasksUI> controller_;
  base::CallbackListSubscription create_services_subscription_;
};

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
                      .params()
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
                      .params()
                      .display_mode(),
                  lens::CobrowsingDisplayModeParams::COBROWSING_SIDEPANEL);
        run_loop.Quit();
      });

  side_panel_controller->OnSidePanelStateChanged();
  run_loop.Run();
}

class ContextualTasksLensBrowserTest : public ContextualTasksUIBrowserTest {
 public:
  ContextualTasksLensBrowserTest() {
    feature_list_.InitAndEnableFeature(contextual_tasks::kContextualTasks);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksLensBrowserTest, HandleLensButtonClick) {
  // Setup LensController
  auto override =
      tabs::TabFeatures::GetUserDataFactoryForTesting().AddOverrideForTesting(
          base::BindLambdaForTesting([&](tabs::TabInterface& tab) {
            auto mock = std::make_unique<MockLensSearchController>(&tab);
            EXPECT_CALL(*mock,
                        OpenLensOverlay(lens::LensOverlayInvocationSource::
                                            kContextualTasksComposebox,
                                        true))
                .Times(1);
            return std::unique_ptr<LensSearchController>(std::move(mock));
          }));

  chrome::NewTab(browser());

  // Bind pipes
  mojo::PendingReceiver<composebox::mojom::PageHandler> handler_receiver;
  mojo::Remote<composebox::mojom::PageHandler> handler_remote(
      handler_receiver.InitWithNewPipeAndPassRemote());

  mojo::PendingRemote<composebox::mojom::Page> composebox_page;
  std::ignore = composebox_page.InitWithNewPipeAndPassReceiver();

  mojo::PendingReceiver<searchbox::mojom::PageHandler>
      searchbox_handler_receiver;
  mojo::PendingRemote<searchbox::mojom::Page> searchbox_page;
  std::ignore = searchbox_page.InitWithNewPipeAndPassReceiver();

  // Create PageHandler
  controller_->CreatePageHandler(
      std::move(composebox_page), std::move(handler_receiver),
      std::move(searchbox_page), std::move(searchbox_handler_receiver));

  // Invoke button click
  handler_remote->HandleLensButtonClick();

  // Flush to ensure message processing on UI thread
  handler_remote.FlushForTesting();
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUIBrowserTest,
                       OnInnerWebContentsCreated_TriggersCookieSync) {
  auto mock_synchronizer = std::make_unique<
      testing::StrictMock<MockContextualTasksCookieSynchronizer>>(
      browser()->profile(), identity_test_env_->identity_manager());

  EXPECT_CALL(*mock_synchronizer, CopyCookiesToWebviewStoragePartition())
      .Times(1);

  controller_->SetCookieSynchronizerForTesting(std::move(mock_synchronizer));

  // Create inner contents to trigger the observer.
  std::unique_ptr<content::WebContents> inner_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));

  TriggerOnInnerWebContentsCreated(inner_contents.get());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUIBrowserTest,
                       OnLensOverlayStateChanged) {
  testing::NiceMock<MockContextualTasksPage> mock_page;

  mojo::PendingReceiver<contextual_tasks::mojom::PageHandler> handler_receiver;
  // The initial call to CreatePageHandler should call
  // OnLensOverlayStateChanged.
  EXPECT_CALL(mock_page, OnLensOverlayStateChanged(
                             /*is_showing=*/false,
                             /*maybe_show_overlay_hint_text=*/false));
  controller_->CreatePageHandler(mock_page.BindAndGetRemote(),
                                 std::move(handler_receiver));

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_page,
                OnLensOverlayStateChanged(
                    /*is_showing=*/true, /*maybe_show_overlay_hint_text=*/true))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    controller_->OnLensOverlayStateChanged(
        true, lens::LensOverlayInvocationSource::kContextualTasksComposebox);
    run_loop.Run();
  }

  {
    base::RunLoop run_loop;
    EXPECT_CALL(mock_page, OnLensOverlayStateChanged(
                               /*is_showing=*/false,
                               /*maybe_show_overlay_hint_text=*/false))
        .WillOnce(testing::InvokeWithoutArgs(&run_loop, &base::RunLoop::Quit));
    controller_->OnLensOverlayStateChanged(
        false, lens::LensOverlayInvocationSource::kAppMenu);
    run_loop.Run();
  }
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksUIBrowserTest,
    OnInnerWebContentsCreated_HandlesMultipleFramesAndReload) {
  // Create first inner contents.
  std::unique_ptr<content::WebContents> inner_contents1 =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  GURL url1 = embedded_test_server()->GetURL("/title1.html?1");
  inner_contents1->GetController().LoadURL(
      url1, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(inner_contents1.get()));
  TriggerOnInnerWebContentsCreated(inner_contents1.get());

  // Verify first inner contents is observed.
  EXPECT_EQ(controller_->GetInnerFrameUrl(), url1);

  // Create second inner contents (should be ignored).
  std::unique_ptr<content::WebContents> inner_contents2 =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  GURL url2 = embedded_test_server()->GetURL("/title1.html?2");
  inner_contents2->GetController().LoadURL(
      url2, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(inner_contents2.get()));
  TriggerOnInnerWebContentsCreated(inner_contents2.get());

  // Verify first inner contents is still observed.
  EXPECT_EQ(controller_->GetInnerFrameUrl(), url1);

  // Navigate the main frame (simulating reload).
  // We use the WebUI's WebContents which is the active tab's WebContents.
  GURL main_url = embedded_test_server()->GetURL("/title1.html?main");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));

  // The navigation observer should have reset the embedded page.
  // Verify embedded page is reset (GetInnerFrameUrl returns empty).
  EXPECT_EQ(controller_->GetInnerFrameUrl(), GURL::EmptyGURL());

  // Create a third inner contents (should be accepted now).
  std::unique_ptr<content::WebContents> inner_contents3 =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  GURL url3 = embedded_test_server()->GetURL("/title1.html?3");
  inner_contents3->GetController().LoadURL(
      url3, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(inner_contents3.get()));
  TriggerOnInnerWebContentsCreated(inner_contents3.get());

  // Verify third inner contents is observed.
  EXPECT_EQ(controller_->GetInnerFrameUrl(), url3);
}

class ContextualTasksNoMockBrowserTest : public InProcessBrowserTest {
 public:
  ContextualTasksNoMockBrowserTest() {
    feature_list_.InitWithFeatures(
        {contextual_tasks::kContextualTasks,
         contextual_tasks::kContextualTasksForceEntryPointEligibility},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksNoMockBrowserTest, CanZoom) {
  EXPECT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(chrome::kChromeUIContextualTasksURL)));
  content::WebContents* web_contents =
      TabListInterface::From(browser())->GetActiveTab()->GetContents();
  auto* zoom_controller = zoom::ZoomController::FromWebContents(web_contents);
  ASSERT_EQ(zoom::ZoomController::ZoomMode::ZOOM_MODE_DEFAULT,
            zoom_controller->zoom_mode());
}
