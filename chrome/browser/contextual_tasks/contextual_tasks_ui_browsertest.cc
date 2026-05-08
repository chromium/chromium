// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/unguessable_token.h"
#include "chrome/browser/autocomplete/aim_eligibility_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_service_factory.h"
#include "chrome/browser/contextual_search/contextual_search_web_contents_helper.h"
#include "chrome/browser/contextual_tasks/contextual_tasks.mojom.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_composebox_handler.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_cookie_synchronizer.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_delegate_desktop.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_service_factory.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_utils.h"
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
#include "components/contextual_search/input_state_model.h"
#include "components/contextual_tasks/public/contextual_task_context.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/contextual_tasks/public/mock_contextual_tasks_service.h"
#include "components/contextual_tasks/public/prefs.h"
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
#include "net/base/url_util.h"
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
  MOCK_METHOD(void, OnSidePanelPinStateChanged, (bool is_pinned), (override));
  MOCK_METHOD(void,
              OnContextUpdated,
              (std::vector<contextual_tasks::mojom::ContextInfoPtr>),
              (override));
  MOCK_METHOD(void, HideInput, (), (override));
  MOCK_METHOD(void, RestoreInput, (), (override));
  MOCK_METHOD(void, EnterBasicMode, (), (override));
  MOCK_METHOD(void, ExitBasicMode, (), (override));
  MOCK_METHOD(void, OnZeroStateChange, (bool is_zero_state), (override));
  MOCK_METHOD(void, SetInNlm, (bool in_nlm), (override));
  MOCK_METHOD(void, OnAiPageStatusChanged, (bool), (override));
  MOCK_METHOD(void,
              OnLensOverlayStateChanged,
              (bool is_showing, bool maybe_show_overlay_hint_text),
              (override));
  MOCK_METHOD(void, SetTaskDetails, (const base::Uuid&), (override));
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
  MOCK_METHOD(void, SetShowReopenTabs, (bool show), (override));
  MOCK_METHOD(void,
              RemoveInjectedInput,
              (const base::UnguessableToken& file_token),
              (override));
  MOCK_METHOD(void,
              InjectInput,
              (contextual_tasks::mojom::InjectedInputPtr input),
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
  ContextualTasksUIBrowserTest() {
    feature_list_.InitAndEnableFeature(contextual_tasks::kContextualTasks);
  }
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
  virtual void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) {
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

  ContextualTasksComposeboxHandler* GetComposeboxHandler() {
    return static_cast<ContextualTasksComposeboxHandler*>(
        controller_->composebox_handler_.get());
  }

  void CallOnContextRetrievedForActiveTab(
      base::WeakPtr<BrowserWindowInterface> browser,
      int32_t tab_id,
      const GURL& last_committed_url,
      std::unique_ptr<contextual_tasks::ContextualTaskContext> context) {
    controller_->OnContextRetrievedForActiveTab(
        browser, tab_id, last_committed_url, std::move(context));
  }

 protected:
  std::unique_ptr<IdentityTestEnvironmentProfileAdaptor>
      identity_test_environment_adaptor_;
  raw_ptr<signin::IdentityTestEnvironment> identity_test_env_;
  std::unique_ptr<content::TestWebUI> test_web_ui_;
  std::unique_ptr<ContextualTasksUI> controller_;
  base::CallbackListSubscription create_services_subscription_;
  base::test::ScopedFeatureList feature_list_;
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

IN_PROC_BROWSER_TEST_F(ContextualTasksUIBrowserTest, HandleLensButtonClick) {
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

class ContextualTasksUICookieSyncBrowserTest
    : public ContextualTasksUIBrowserTest {
 public:
  void OnWillCreateBrowserContextServices(
      content::BrowserContext* context) override {
    ContextualTasksUIBrowserTest::OnWillCreateBrowserContextServices(context);
    contextual_tasks::ContextualTasksUiServiceFactory::GetInstance()
        ->SetTestingFactory(
            context,
            base::BindRepeating(
                &ContextualTasksUICookieSyncBrowserTest::CreateMockUiService,
                base::Unretained(this)));
  }

  void TearDownOnMainThread() override {
    ContextualTasksUIBrowserTest::TearDownOnMainThread();
    mock_synchronizer_ = nullptr;
  }

  std::unique_ptr<KeyedService> CreateMockUiService(
      content::BrowserContext* context) {
    Profile* profile = Profile::FromBrowserContext(context);
    auto delegate = std::make_unique<
        contextual_tasks::ContextualTasksUiServiceDelegateDesktop>(profile);
    auto mock = std::make_unique<
        testing::NiceMock<MockContextualTasksCookieSynchronizer>>(
        profile, IdentityManagerFactory::GetForProfile(profile));
    mock_synchronizer_ = mock.get();
    return std::make_unique<contextual_tasks::ContextualTasksUiService>(
        profile, std::move(delegate),
        contextual_tasks::ContextualTasksServiceFactory::GetForProfile(profile),
        IdentityManagerFactory::GetForProfile(profile),
        AimEligibilityServiceFactory::GetForProfile(profile), std::move(mock));
  }

 protected:
  raw_ptr<MockContextualTasksCookieSynchronizer> mock_synchronizer_ = nullptr;
};

IN_PROC_BROWSER_TEST_F(ContextualTasksUICookieSyncBrowserTest,
                       OnInnerWebContentsCreated_TriggersCookieSync) {
  EXPECT_CALL(*mock_synchronizer_, CopyCookiesToWebviewStoragePartition())
      .Times(1);

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

IN_PROC_BROWSER_TEST_F(ContextualTasksUIBrowserTest,
                       CanUpdateSuggestedTabContext_ValidSchemes) {
  tabs::TabInterface* tab = TabListInterface::From(browser())->GetActiveTab();
  ASSERT_TRUE(tab);

  // No composebox_handler_ initialized yet.
  EXPECT_FALSE(controller_->CanUpdateSuggestedTabContext(
      tab, GURL("http://example.com")));

  mojo::PendingReceiver<composebox::mojom::PageHandler> handler_receiver;
  mojo::Remote<composebox::mojom::PageHandler> handler_remote(
      handler_receiver.InitWithNewPipeAndPassRemote());
  mojo::PendingRemote<composebox::mojom::Page> composebox_page;
  std::ignore = composebox_page.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<searchbox::mojom::PageHandler>
      searchbox_handler_receiver;
  mojo::PendingRemote<searchbox::mojom::Page> searchbox_page;
  std::ignore = searchbox_page.InitWithNewPipeAndPassReceiver();

  controller_->CreatePageHandler(
      std::move(composebox_page), std::move(handler_receiver),
      std::move(searchbox_page), std::move(searchbox_handler_receiver));

  // Should succeed for http/https/file URLs.
  EXPECT_TRUE(controller_->CanUpdateSuggestedTabContext(
      tab, GURL("http://example.com")));
  EXPECT_TRUE(controller_->CanUpdateSuggestedTabContext(
      tab, GURL("https://example.com")));
  EXPECT_TRUE(controller_->CanUpdateSuggestedTabContext(
      tab, GURL("file:///tmp/test.txt")));

  // Should fail for other schemes.
  EXPECT_FALSE(controller_->CanUpdateSuggestedTabContext(
      tab, GURL("chrome://settings")));
  EXPECT_FALSE(controller_->CanUpdateSuggestedTabContext(
      tab, GURL("data:text/html,test")));
  EXPECT_FALSE(
      controller_->CanUpdateSuggestedTabContext(tab, GURL("about:blank")));

  // Should fail if tab is null.
  EXPECT_FALSE(controller_->CanUpdateSuggestedTabContext(
      nullptr, GURL("http://example.com")));

  // Should fail for invalid URL.
  EXPECT_FALSE(controller_->CanUpdateSuggestedTabContext(tab, GURL()));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUIBrowserTest,
                       CanUpdateSuggestedTabContext_SiteExclusion) {
  tabs::TabInterface* tab = TabListInterface::From(browser())->GetActiveTab();
  ASSERT_TRUE(tab);

  // No composebox_handler_ initialized yet.
  EXPECT_FALSE(controller_->CanUpdateSuggestedTabContext(
      tab, GURL("http://example.com")));

  mojo::PendingReceiver<composebox::mojom::PageHandler> handler_receiver;
  mojo::Remote<composebox::mojom::PageHandler> handler_remote(
      handler_receiver.InitWithNewPipeAndPassRemote());
  mojo::PendingRemote<composebox::mojom::Page> composebox_page;
  std::ignore = composebox_page.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<searchbox::mojom::PageHandler>
      searchbox_handler_receiver;
  mojo::PendingRemote<searchbox::mojom::Page> searchbox_page;
  std::ignore = searchbox_page.InitWithNewPipeAndPassReceiver();

  controller_->CreatePageHandler(
      std::move(composebox_page), std::move(handler_receiver),
      std::move(searchbox_page), std::move(searchbox_handler_receiver));

  // Add a couple of exclusions and save to prefs.
  base::Time now = base::Time::Now();
  base::DictValue site_exclusions;
  site_exclusions.Set("excluded.com",
                      static_cast<double>(now.InMillisecondsSinceUnixEpoch()));
  contextual_tasks::SaveSiteExclusionsToPrefs(GetProfile()->GetPrefs(),
                                              site_exclusions);

  EXPECT_TRUE(controller_->CanUpdateSuggestedTabContext(
      tab, GURL("http://example.com")));
  EXPECT_FALSE(controller_->CanUpdateSuggestedTabContext(
      tab, GURL("http://excluded.com")));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUIBrowserTest,
                       RecordsHttpResponseCodeHistograms) {
  base::HistogramTester histogram_tester;

  // Create inner contents to trigger the observer.
  std::unique_ptr<content::WebContents> inner_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  TriggerOnInnerWebContentsCreated(inner_contents.get());

  GURL url = embedded_test_server()->GetURL("/title1.html");
  inner_contents->GetController().LoadURL(
      url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(inner_contents.get()));

  // Since the URL is not an AI URL, IsZeroState will return false.
  // Both histograms should be recorded.
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.InnerFrameContents.HttpResponseCode", 200, 1);
  histogram_tester.ExpectUniqueSample(
      "ContextualTasks.InnerFrameContents.HttpResponseCode."
      "ExcludeZeroStateLoads",
      200, 1);
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

IN_PROC_BROWSER_TEST_F(ContextualTasksNoMockBrowserTest,
                       CannotZoomInSidePanel) {
  std::unique_ptr<content::WebContents> side_panel_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));
  auto side_panel_web_ui = std::make_unique<content::TestWebUI>();
  side_panel_web_ui->set_web_contents(side_panel_contents.get());

  auto side_panel_controller =
      std::make_unique<ContextualTasksUI>(side_panel_web_ui.get());

  static_cast<content::WebUIController*>(side_panel_controller.get())
      ->WebUIPrimaryPageChanged(side_panel_contents->GetPrimaryPage());

  auto* zoom_controller =
      zoom::ZoomController::FromWebContents(side_panel_contents.get());
  ASSERT_EQ(zoom::ZoomController::ZoomMode::ZOOM_MODE_DISABLED,
            zoom_controller->zoom_mode());
}

IN_PROC_BROWSER_TEST_F(ContextualTasksNoMockBrowserTest,
                       InitSidePanelWithGhostLoader_WaitUntilPanelOpen) {
  auto* service =
      contextual_tasks::ContextualTasksUiServiceFactory::GetForBrowserContext(
          browser()->profile());
  auto* tab = TabListInterface::From(browser())->GetActiveTab();

  // Call InitSidePanelWithGhostLoader.
  service->InitSidePanelWithGhostLoader(browser(), tab, nullptr);

  // Wait for side panel to open and load WebUI.
  auto* controller =
      contextual_tasks::ContextualTasksPanelController::From(browser());
  ASSERT_TRUE(controller);
  EXPECT_TRUE(base::test::RunUntil(
      [&]() { return controller->IsPanelOpenForContextualTask(); }));

  content::WebContents* web_contents = controller->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Wait for load stop on that web_contents.
  EXPECT_TRUE(content::WaitForLoadStop(web_contents));
}

IN_PROC_BROWSER_TEST_F(ContextualTasksUIBrowserTest,
                       UpdateModelFromUrlOnNavigation) {
  omnibox::SearchboxConfig config;

  auto* fast_config = config.add_model_configs();
  fast_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  fast_config->mutable_rule()->set_model(
      omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);
  auto* fast_param = fast_config->add_aim_url_params();
  fast_param->set_param_key("udm");
  fast_param->set_param_value("50");

  auto* pro_config = config.add_model_configs();
  pro_config->set_model(omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  pro_config->mutable_rule()->set_model(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);
  auto* pro_param1 = pro_config->add_aim_url_params();
  pro_param1->set_param_key("udm");
  pro_param1->set_param_value("50");
  auto* pro_param2 = pro_config->add_aim_url_params();
  pro_param2->set_param_key("arv");
  pro_param2->set_param_value("1");

  auto* sibling_config = config.add_model_configs();
  sibling_config->set_model(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE);
  sibling_config->mutable_rule()->set_model(
      omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE);
  auto* sib_param1 = sibling_config->add_aim_url_params();
  sib_param1->set_param_key("udm");
  sib_param1->set_param_value("50");
  auto* sib_param2 = sibling_config->add_aim_url_params();
  sib_param2->set_param_key("xyz");
  sib_param2->set_param_value("1");

  auto* contextual_search_service =
      ContextualSearchServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(contextual_search_service);

  auto session_handle = contextual_search_service->CreateSession(
      contextual_tasks::CreateQueryControllerConfigParams(),
      contextual_search::ContextualSearchSource::kContextualTasks,
      /*invocation_source=*/std::nullopt);
  ASSERT_TRUE(session_handle);

  GURL initial_url("https://example.com/");
  auto input_state_model = std::make_unique<contextual_search::InputStateModel>(
      *session_handle, config, initial_url, /*is_off_the_record=*/false);

  content::WebContents* web_contents =
      TabListInterface::From(browser())->GetActiveTab()->GetContents();
  auto* helper = ContextualSearchWebContentsHelper::GetOrCreateForWebContents(
      web_contents);

  mojo::PendingRemote<contextual_tasks::mojom::Page> base_page;
  auto base_page_receiver = base_page.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<contextual_tasks::mojom::PageHandler>
      base_handler_receiver;
  controller_->CreatePageHandler(std::move(base_page),
                                 std::move(base_handler_receiver));

  base::Uuid task_id = base::Uuid::GenerateRandomV4();
  controller_->SetTaskId(task_id);

  helper->SetTaskSession(task_id, std::move(session_handle),
                         std::move(input_state_model));

  mojo::PendingReceiver<composebox::mojom::PageHandler> handler_receiver;
  mojo::Remote<composebox::mojom::PageHandler> handler_remote(
      handler_receiver.InitWithNewPipeAndPassRemote());
  mojo::PendingRemote<composebox::mojom::Page> composebox_page;
  std::ignore = composebox_page.InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<searchbox::mojom::PageHandler>
      searchbox_handler_receiver;
  mojo::PendingRemote<searchbox::mojom::Page> searchbox_page;
  std::ignore = searchbox_page.InitWithNewPipeAndPassReceiver();

  controller_->CreatePageHandler(
      std::move(composebox_page), std::move(handler_receiver),
      std::move(searchbox_page), std::move(searchbox_handler_receiver));

  controller_->OnInitComplete();

  std::unique_ptr<content::WebContents> inner_contents =
      content::WebContents::Create(
          content::WebContents::CreateParams(browser()->profile()));

  TriggerOnInnerWebContentsCreated(inner_contents.get());

  auto* handler = GetComposeboxHandler();
  ASSERT_TRUE(handler);
  ASSERT_TRUE(handler->input_state_model());

  // Navigate with Fast model parameters.
  GURL fast_url = embedded_test_server()->GetURL("/title1.html?udm=50");
  inner_contents->GetController().LoadURL(
      fast_url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(inner_contents.get()));

  EXPECT_EQ(handler->input_state_model()->get_state_for_testing().active_model,
            omnibox::ModelMode::MODEL_MODE_GEMINI_REGULAR);

  // Navigate with Pro model parameters.
  GURL pro_url = embedded_test_server()->GetURL("/title1.html?udm=50&arv=1");
  inner_contents->GetController().LoadURL(
      pro_url, content::Referrer(), ui::PAGE_TRANSITION_TYPED, std::string());
  EXPECT_TRUE(content::WaitForLoadStop(inner_contents.get()));

  EXPECT_EQ(handler->input_state_model()->get_state_for_testing().active_model,
            omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);

  // Permutation Reversal Navigations
  GURL reversed_pro_url =
      embedded_test_server()->GetURL("/title1.html?arv=1&udm=50");
  inner_contents->GetController().LoadURL(reversed_pro_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_TYPED,
                                          std::string());
  EXPECT_TRUE(content::WaitForLoadStop(inner_contents.get()));
  EXPECT_EQ(handler->input_state_model()->get_state_for_testing().active_model,
            omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);

  // Sibling ambiguity rank Navigations
  GURL ambiguous_url =
      embedded_test_server()->GetURL("/title1.html?udm=50&arv=1&xyz=1");
  inner_contents->GetController().LoadURL(ambiguous_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_TYPED,
                                          std::string());
  EXPECT_TRUE(content::WaitForLoadStop(inner_contents.get()));
  EXPECT_EQ(handler->input_state_model()->get_state_for_testing().active_model,
            omnibox::ModelMode::MODEL_MODE_GEMINI_PRO);

  // Differentiating sibling specificity Navigations
  GURL sibling_url =
      embedded_test_server()->GetURL("/title1.html?udm=50&xyz=1");
  inner_contents->GetController().LoadURL(sibling_url, content::Referrer(),
                                          ui::PAGE_TRANSITION_TYPED,
                                          std::string());
  EXPECT_TRUE(content::WaitForLoadStop(inner_contents.get()));
  EXPECT_EQ(handler->input_state_model()->get_state_for_testing().active_model,
            omnibox::ModelMode::MODEL_MODE_GEMINI_PRO_AUTOROUTE);
}

IN_PROC_BROWSER_TEST_F(
    ContextualTasksUIBrowserTest,
    OnContextRetrievedForActiveTab_NullBrowser_DoesNotCrash) {
  // Call OnContextRetrievedForActiveTab with a null weak pointer.
  base::WeakPtr<BrowserWindowInterface> null_browser;
  int32_t tab_id = 1;
  GURL url("https://example.com");
  contextual_tasks::ContextualTask task(base::Uuid::GenerateRandomV4());
  auto context =
      std::make_unique<contextual_tasks::ContextualTaskContext>(task);

  // This should return early and not crash.
  CallOnContextRetrievedForActiveTab(null_browser, tab_id, url,
                                     std::move(context));
}
