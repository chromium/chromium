// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/statistics_recorder.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/simple_test_tick_clock.h"
#include "base/test/test_future.h"
#include "build/build_config.h"
#include "chrome/browser/extensions/chrome_extension_web_contents_observer.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_controller.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_dialog_view.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_page_handler.h"
#include "chrome/browser/glic/experimental_opt_in/glic_experimental_opt_in_util.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/host/auth_controller.h"
#include "chrome/browser/glic/host/glic_cookie_synchronizer.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/public/glic_keyed_service_factory.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/sync/device_info_sync_service_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/browser_window/public/create_browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_device_info/fake_device_info_sync_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/base/base_window.h"
#include "ui/base/page_transition_types.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace glic {

class GlicExperimentalOptInTest
    : public GlicBrowserTestMixin<MixinBasedInProcessBrowserTest> {
 public:
  using BaseClass = GlicBrowserTestMixin<MixinBasedInProcessBrowserTest>;
  using MixinBasedInProcessBrowserTest::browser;
  GlicExperimentalOptInTest() : GlicExperimentalOptInTest(GURL()) {}
  explicit GlicExperimentalOptInTest(GURL opt_in_url)
      : opt_in_url_(std::move(opt_in_url)) {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(base::BindRepeating(
                &GlicExperimentalOptInTest::OnWillCreateBrowserContextServices,
                base::Unretained(this)));
  }
  ~GlicExperimentalOptInTest() override = default;

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    DeviceInfoSyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          auto service = std::make_unique<syncer::FakeDeviceInfoSyncService>();
          service->GetLocalDeviceInfoProvider()->UpdateClientName(
              "My Test Device");
          return service;
        }));
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          return std::make_unique<syncer::TestSyncService>();
        }));
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    BaseClass::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        network::switches::kHostResolverRules,
        "MAP accounts.google.com " +
            fake_gaia_.gaia_server()->host_port_pair().ToString());
  }

  void SetUp() override {
    net::EmbeddedTestServer::ServerCertificateConfig cert_config;
    cert_config.dns_names = {"google.fr"};
    embedded_https_test_server().SetSSLConfig(cert_config);
    opt_in_test_server_.ServeFilesFromSourceDirectory(
        "chrome/test/data/webui/glic/");
    ASSERT_TRUE(opt_in_test_server_.InitializeAndListen());
    GURL test_url;
    if (!opt_in_url_.is_empty()) {
      test_url = opt_in_url_;
    } else {
      test_url = opt_in_test_server_.GetURL("a.test", "/test_data/page.html");
    }

    std::vector<base::test::FeatureRefAndParams> enabled_features;

    base::FieldTrialParams params;
    params[features::kGlicExperimentalTriggeringOptInURL.name] =
        test_url.spec();
    enabled_features.push_back({features::kGlicExperimentalTriggering, params});

    base::FieldTrialParams tab_focus_params;
    tab_focus_params[features::kGlicExperimentalTriggeringTabFocusHosts.name] =
        "a.test";
    tab_focus_params[features::kGlicExperimentalTriggeringTabFocusPathSubstring
                         .name] = "/test_data";
    tab_focus_params[features::kGlicExperimentalTriggeringTabFocusFallbackURL
                         .name] =
        opt_in_test_server_.GetURL("a.test", "/test_data/fallback.html").spec();
    enabled_features.push_back(
        {features::kGlicExperimentalTriggeringOptInTabFocus, tab_focus_params});

    feature_list_.InitWithFeaturesAndParameters(enabled_features, {});

    BaseClass::SetUp();
  }

  void SetUpOnMainThread() override {
    fake_gaia_.set_initialize_configuration(false);
    BaseClass::SetUpOnMainThread();
    opt_in_test_server_.StartAcceptingConnections();
    ASSERT_TRUE(embedded_https_test_server().Start());
    host_resolver()->AddRule("*", "127.0.0.1");
    creation_subscription_ = content::RegisterWebContentsCreationCallback(
        base::BindRepeating(&GlicExperimentalOptInTest::OnWebContentsCreated,
                            base::Unretained(this)));

    fake_gaia_.SetupFakeGaiaForLoginWithDefaults();
    FakeGaia::Configuration config;
    config.emails = {"glic-test@example.com"};
    config.session_sid_cookie = FakeGaiaMixin::kFakeSIDCookie;
    config.session_lsid_cookie = FakeGaiaMixin::kFakeLSIDCookie;
    fake_gaia_.fake_gaia()->UpdateConfiguration(config);

    signin::SetAutomaticIssueOfAccessTokens(
        IdentityManagerFactory::GetForProfile(browser()->GetProfile()), true);
  }

  guest_view::TestGuestViewManager* GetGuestViewManager() {
    return guest_view_manager_factory_.GetOrCreateTestGuestViewManager(
        browser()->GetProfile(), extensions::ExtensionsAPIClient::Get()
                                     ->CreateGuestViewManagerDelegate());
  }

  views::Widget* ShowDialogAndWait(
      content::WebContents* web_contents = nullptr,
      base::OnceCallback<void(bool)> callback = base::DoNothing()) {
    if (!web_contents) {
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    }
    service()->opt_in_controller().ShowDialog(web_contents,
                                              std::move(callback));
    views::Widget* widget =
        service()->opt_in_controller().GetDialogWidgetForTesting();
    if (!widget) {
      return nullptr;
    }
    if (auto* tab = tabs::TabInterface::MaybeGetFromContents(web_contents)) {
      if (auto* window = tab->GetBrowserWindowInterface()) {
        auto* last_active =
            GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser();
        if (last_active != window) {
          ui_test_utils::BrowserDidBecomeActiveWaiter waiter(window);
          waiter.Wait();
        }
      }
    }
    views::test::WidgetVisibleWaiter(widget).Wait();
    EXPECT_TRUE(widget->IsVisible());
    return widget;
  }

  content::WebContents* WaitForGuestContents(bool expect_success = true) {
    auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
    if (!guest_view) {
      return nullptr;
    }
    content::WebContents* guest_contents = guest_view->web_contents();
    if (!guest_contents) {
      return nullptr;
    }
    bool load_success = content::WaitForLoadStop(guest_contents);
    if (expect_success) {
      EXPECT_TRUE(load_success);
    }
    return guest_contents;
  }

  views::WebView* GetDialogWebView() {
    GlicExperimentalOptInDialogView* dialog_view =
        service()->opt_in_controller().GetDialogViewForTesting();
    return dialog_view ? dialog_view->GetWebViewForTesting() : nullptr;
  }

  content::WebContents* GetDialogWebContents() {
    views::WebView* web_view = GetDialogWebView();
    return web_view ? web_view->GetWebContents() : nullptr;
  }

  static constexpr char kCommonJs[] = R"js(
function getElement(name) { return document.getElementById(name); }
function getRequiredElement(name) {
  const e = getElement(name);
  if (!e) { throw new Error(`Could not find element: ${name}`); }
  return e;
}
function getErrorPanel() { return getRequiredElement('errorPanel'); }
function getTryAgainButton() { return getRequiredElement('tryAgainButton'); }
function getCloseButtonError() { return getRequiredElement('closeButtonError'); }
function getWebview() { return getElement('webview'); }
function isVisible(e) { return !!(e && !e.hidden && window.getComputedStyle(e).display !== 'none'); }
function isHidden(e) { return !!(e && e.hidden); }
)js";

  content::EvalJsResult EvalJs(
      const content::ToRenderFrameHost& execution_target,
      const std::string& js_code) {
    return content::EvalJs(execution_target, std::string(kCommonJs) + js_code);
  }

  [[nodiscard]] testing::AssertionResult ExecJs(
      const content::ToRenderFrameHost& execution_target,
      const std::string& js_code) {
    return content::ExecJs(execution_target, std::string(kCommonJs) + js_code);
  }

  void VerifyWebviewURLForState(const std::string& expected_state_value) {
    views::Widget* widget = ShowDialogAndWait();
    ASSERT_TRUE(widget);

    content::WebContents* guest_contents = WaitForGuestContents();
    ASSERT_TRUE(guest_contents);

    ASSERT_TRUE(guest_contents->GetController().GetLastCommittedEntry());
    EXPECT_EQ(
        guest_contents->GetController().GetLastCommittedEntry()->GetPageType(),
        content::PAGE_TYPE_NORMAL);
    GURL actual_url = guest_contents->GetLastCommittedURL();

    GURL expected_url =
        opt_in_test_server_.GetURL("a.test", "/test_data/page.html");
    expected_url = net::AppendOrReplaceQueryParameter(
        expected_url, "experimental_triggering_opt_in", expected_state_value);
    expected_url = DecorateGlicOptInUrl(browser()->GetProfile(), expected_url);
    EXPECT_EQ(actual_url, expected_url);

    service()->opt_in_controller().CloseDialog(false);
  }

  FakeGaiaMixin& fake_gaia() { return fake_gaia_; }

 private:
  // In a stripped-down browser test environment, dynamically created guest
  // WebContents inside <webview> lack critical extensions observers. We
  // manually attach them here to enable IPC bindings and navigation success.
  void OnWebContentsCreated(content::WebContents* web_contents) {
    extensions::ChromeExtensionWebContentsObserver::CreateForWebContents(
        web_contents);
    extensions::TabHelper::CreateForWebContents(web_contents);
  }

  GURL opt_in_url_;
  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription creation_subscription_;
  net::EmbeddedTestServer opt_in_test_server_;
  guest_view::TestGuestViewManagerFactory guest_view_manager_factory_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, OpensDialog) {
  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  content::WebContents* dialog_contents = GetDialogWebContents();
  ASSERT_TRUE(dialog_contents);

  EXPECT_TRUE(content::WaitForLoadStop(dialog_contents));
  ASSERT_TRUE(dialog_contents->GetController().GetLastCommittedEntry());
  EXPECT_EQ(
      dialog_contents->GetController().GetLastCommittedEntry()->GetPageType(),
      content::PAGE_TYPE_NORMAL);
  EXPECT_EQ(dialog_contents->GetLastCommittedURL(),
            GURL(chrome::kChromeUIGlicExperimentalOptInURL));

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);
  EXPECT_TRUE(content::WaitForLoadStop(guest_contents));

  GURL webview_url = guest_contents->GetLastCommittedURL();
  std::string device_name_param;
  EXPECT_TRUE(net::GetValueForKeyInQuery(webview_url, "device_name",
                                         &device_name_param));
  EXPECT_EQ(device_name_param, "My Test Device");

  service()->opt_in_controller().CloseDialog(false);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       RecordsOptInDialogShowDuration) {
  base::HistogramTester histogram_tester;

  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  service()->opt_in_controller().CloseDialog(false);

  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.OptInDialog.ShowDuration", 1);
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.OptInDialog.VisibleDuration", 1);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       RecordsOptInDialogShowDurationIgnoresBackgroundTime) {
  base::SimpleTestTickClock test_clock;
  test_clock.Advance(base::Seconds(1));
  service()->opt_in_controller().SetTickClockForTesting(&test_clock);

  base::HistogramTester histogram_tester;

  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  views::Widget* widget = ShowDialogAndWait(tab1);
  ASSERT_TRUE(widget);

  // Open a new tab, causing tab1 (and the dialog) to enter the background.
  chrome::AddSelectedTabWithURL(browser(), GURL("about:blank"),
                                ui::PAGE_TRANSITION_LINK);
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(tab1, tab2);

  views::test::WidgetVisibleWaiter(widget).WaitUntilInvisible();
  EXPECT_FALSE(widget->IsVisible());

  // Advance simulated time while the dialog is hidden in the background tab.
  test_clock.Advance(base::Milliseconds(1500));

  // Close the dialog while still on tab2.
  service()->opt_in_controller().CloseDialog(false);

  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.OptInDialog.ShowDuration", 1);
  histogram_tester.ExpectTotalCount(
      "Glic.ExperimentalTriggering.OptInDialog.VisibleDuration", 1);

  int64_t show_duration_ms = histogram_tester.GetTotalSum(
      "Glic.ExperimentalTriggering.OptInDialog.ShowDuration");
  EXPECT_GE(show_duration_ms, 1500);

  int64_t visible_duration_ms = histogram_tester.GetTotalSum(
      "Glic.ExperimentalTriggering.OptInDialog.VisibleDuration");
  // The 1.5 seconds spent in the background tab should not be counted.
  EXPECT_LT(visible_duration_ms, 1000);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, TabModality) {
  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetActiveWebContents();

  views::Widget* widget = ShowDialogAndWait(tab1);
  ASSERT_TRUE(widget);

  // Open a new tab.
  chrome::AddSelectedTabWithURL(browser(), GURL("about:blank"),
                                ui::PAGE_TRANSITION_LINK);
  content::WebContents* tab2 =
      browser()->tab_strip_model()->GetActiveWebContents();
  EXPECT_NE(tab1, tab2);

  // The dialog should be hidden.
  views::test::WidgetVisibleWaiter(widget).WaitUntilInvisible();
  EXPECT_FALSE(widget->IsVisible());

  // Switch back to tab1.
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_EQ(tab1, browser()->tab_strip_model()->GetActiveWebContents());

  // The dialog should be visible again.
  views::test::WidgetVisibleWaiter(widget).Wait();
  EXPECT_TRUE(widget->IsVisible());

  // Cleanup.
  service()->opt_in_controller().CloseDialog(false);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, WebviewURL_GlicOptInState) {
  // Set FRE to incomplete to ensure HasConsented is false.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(service()->enabling().HasConsented());

  VerifyWebviewURLForState("glic");
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       WebviewURL_ActuationOptInState) {
  // Set Glic FRE completed, but actuation disabled.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service()->enabling().SetUserEnabledActuationOnWeb(false);

  VerifyWebviewURLForState("actuation");
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       WebviewURL_ExperimentalOptInState) {
  // Set Glic FRE completed, actuation enabled, but experimental triggering
  // disabled.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service()->enabling().SetUserEnabledActuationOnWeb(true);
  service()->enabling().SetExperimentalTriggeringEnabled(false);

  VerifyWebviewURLForState("experimental");
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, WebviewURL_OptInNotNeeded) {
  // Set Glic FRE completed, actuation enabled, AND experimental triggering
  // enabled.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service()->enabling().SetUserEnabledActuationOnWeb(true);
  service()->enabling().SetExperimentalTriggeringEnabled(true);

  // Verify ShowDialog returns nullptr since opt-in is already complete!
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  base::test::TestFuture<bool> future;
  service()->opt_in_controller().ShowDialog(web_contents, future.GetCallback());
  views::Widget* widget =
      service()->opt_in_controller().GetDialogWidgetForTesting();
  EXPECT_FALSE(widget);
  EXPECT_TRUE(future.Get());
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, ResizesToContent) {
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "a.test") {
          std::string html = R"(
            <html>
              <body style="width: 500px; height: 400px; margin: 0;">
                <div style="width: 100%; height: 100%; background: blue;"></div>
              </body>
            </html>
          )";
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-type: text/html\n\n", html,
              params->client.get());
          return true;
        }
        return false;
      }));

  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  views::WebView* web_view = GetDialogWebView();
  ASSERT_TRUE(web_view);

  // Verify that the WebView's preferred size resizes to match the loaded
  // content.
  bool size_matched = base::test::RunUntil([web_view]() {
    return web_view->GetPreferredSize() == gfx::Size(512, 400);
  });

  EXPECT_TRUE(size_matched) << "Timed out waiting for web_view to resize to "
                               "512x400! Current preferred size: "
                            << web_view->GetPreferredSize().ToString();

  service()->opt_in_controller().CloseDialog(false);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       TabClosedClosesDialogSynchronously) {
  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  views::WebView* web_view = GetDialogWebView();
  ASSERT_TRUE(web_view);

  // Track the view and widget lifetime.
  views::ViewTracker tracker(web_view);
  EXPECT_TRUE(tracker.view());
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);

  // Close the tab (triggers dialog close).
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  // The view and widget should be destroyed synchronously.
  EXPECT_FALSE(tracker.view());
  destroyed_waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, TabDraggedToAnotherWindow) {
  // Add a second tab so we may detach the first.
  chrome::AddTabAt(browser(), GURL(url::kAboutBlankURL), 1, false);

  content::WebContents* tab1 =
      browser()->tab_strip_model()->GetWebContentsAt(0);

  views::Widget* widget = ShowDialogAndWait(tab1);
  ASSERT_TRUE(widget);

  views::WebView* web_view = GetDialogWebView();
  ASSERT_TRUE(web_view);

  // Track the view and widget lifetime.
  views::ViewTracker tracker(web_view);
  EXPECT_TRUE(tracker.view());
  views::test::WidgetDestroyedWaiter destroyed_waiter(widget);

  // Detach the tab (triggers kInsertIntoOtherWindow detach reason and sync
  // dialog close).
  std::unique_ptr<content::WebContents> detached_contents =
      browser()->tab_strip_model()->DetachWebContentsAtForInsertion(0);

  // The view and widget should be destroyed synchronously due to
  // MakeCloseSynchronous in the controller.
  EXPECT_FALSE(tracker.view());
  destroyed_waiter.Wait();

  // Re-insert the detached WebContents into the tab strip so it is cleanly
  // destroyed during browser teardown.
  browser()->tab_strip_model()->AppendWebContents(std::move(detached_contents),
                                                  true);
}
IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, AcceptOptInGlic) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  // Set required state to Glic.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(service()->enabling().HasConsented());

  base::test::TestFuture<bool> opt_in_result;
  views::Widget* widget =
      ShowDialogAndWait(nullptr, opt_in_result.GetCallback());
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  // Verify Shown metrics
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Glic.ExperimentalTriggering.OptIn.Glic.Shown"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.Shown"), 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.OptIn.Shown.Version",
      RequiredExperimentalOptIn::kGlic, 1);
  histogram_tester.ExpectUniqueSample("Glic.Fre.Shown.FlowSource",
                                      OptInFlow::kExperimentalTriggering, 1);

  // Change location hash to #continue to simulate user accepting the opt-in.
  ASSERT_TRUE(ExecJs(guest_contents, "window.location.hash = '#continue';"));

  // Wait for the widget to close.
  views::test::WidgetDestroyedWaiter(widget).Wait();

  EXPECT_TRUE(opt_in_result.Get());

  // Verify that Glic is consented, Actuation is enabled, AND Experimental is
  // enabled (3 opt-ins).
  EXPECT_TRUE(service()->enabling().HasConsented());
  EXPECT_TRUE(service()->enabling().GetUserEnabledActuationOnWeb());
  EXPECT_TRUE(service()->enabling().GetExperimentalTriggeringEnabled());

  // Verify Accept metrics
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Onboarding.OptInAccept"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Glic.ExperimentalTriggering.OptIn.Glic.Accepted"),
            1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.OptIn.Accepted.Version",
      RequiredExperimentalOptIn::kGlic, 1);
  histogram_tester.ExpectUniqueSample("Glic.Fre.Accept.FlowSource",
                                      OptInFlow::kExperimentalTriggering, 1);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, RejectOptIn) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(service()->enabling().HasConsented());

  base::test::TestFuture<bool> opt_in_result;
  views::Widget* widget =
      ShowDialogAndWait(nullptr, opt_in_result.GetCallback());
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  // Verify Shown metrics
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Glic.ExperimentalTriggering.OptIn.Glic.Shown"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.Shown"), 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.OptIn.Shown.Version",
      RequiredExperimentalOptIn::kGlic, 1);
  histogram_tester.ExpectUniqueSample("Glic.Fre.Shown.FlowSource",
                                      OptInFlow::kExperimentalTriggering, 1);

  // Change location hash to #noThanks.
  ASSERT_TRUE(ExecJs(guest_contents, "window.location.hash = '#noThanks';"));

  // Wait for the widget to close.
  views::test::WidgetDestroyedWaiter(widget).Wait();

  EXPECT_FALSE(opt_in_result.Get());

  // Verify Glic is still not consented.
  EXPECT_FALSE(service()->enabling().HasConsented());

  // Verify Reject metrics
  EXPECT_EQ(user_action_tester.GetActionCount("Glic.Fre.NoThanks"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Glic.ExperimentalTriggering.OptIn.Glic.NoThanks"),
            1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.OptIn.NoThanks.Version",
      RequiredExperimentalOptIn::kGlic, 1);
  histogram_tester.ExpectUniqueSample("Glic.Fre.NoThanks.FlowSource",
                                      OptInFlow::kExperimentalTriggering, 1);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, RejectThenAcceptOptIn) {
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(service()->enabling().HasConsented());

  // Capture the dialog result to verify it gets rejected upon closing.
  base::test::TestFuture<bool> opt_in_result;
  views::Widget* widget =
      ShowDialogAndWait(nullptr, opt_in_result.GetCallback());
  ASSERT_TRUE(widget);

  content::WebContents* dialog_contents = WaitForGuestContents();

  // 1. Reject first dialog
  views::test::WidgetDestroyedWaiter waiter(widget);
  ASSERT_TRUE(ExecJs(dialog_contents, "window.location.hash = '#noThanks';"));

  // The dialog should close and the result should be false (rejected).
  waiter.Wait();
  EXPECT_FALSE(opt_in_result.Get());
  EXPECT_FALSE(service()->enabling().HasConsented());

  // 2. Show second dialog
  base::test::TestFuture<bool> opt_in_result2;
  views::Widget* widget2 =
      ShowDialogAndWait(nullptr, opt_in_result2.GetCallback());
  ASSERT_TRUE(widget2);

  auto* guest_view2 = GetGuestViewManager()->WaitForNextGuestViewCreated();
  ASSERT_TRUE(guest_view2);
  content::WebContents* dialog_contents2 = guest_view2->web_contents();
  ASSERT_TRUE(dialog_contents2);
  EXPECT_TRUE(content::WaitForLoadStop(dialog_contents2));

  // 3. Accept second dialog
  views::test::WidgetDestroyedWaiter waiter2(widget2);
  ASSERT_TRUE(ExecJs(dialog_contents2, "window.location.hash = '#continue';"));

  // The dialog should close and the result should be true (accepted).
  waiter2.Wait();
  EXPECT_TRUE(opt_in_result2.Get());
  EXPECT_TRUE(service()->enabling().HasConsented());
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       GlicOptInImpressionMetricRecordedOnLoad) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(service()->enabling().HasConsented());

  base::StatisticsRecorder::HistogramWaiter waiter(
      "Glic.Onboarding.OptInImpression.FlowSource");

  base::test::TestFuture<bool> opt_in_result;
  views::Widget* widget =
      ShowDialogAndWait(nullptr, opt_in_result.GetCallback());
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  waiter.Wait();

  EXPECT_EQ(
      user_action_tester.GetActionCount("Glic.Onboarding.OptInImpression"), 1);
  histogram_tester.ExpectUniqueSample(
      "Glic.Onboarding.OptInImpression.FlowSource",
      OptInFlow::kExperimentalTriggering, 1);

  service()->opt_in_controller().CloseDialog(false);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, AcceptOptInActuation) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  // Set required state to Actuation (Glic complete, Actuation incomplete).
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service()->enabling().SetUserEnabledActuationOnWeb(false);
  ASSERT_TRUE(service()->enabling().HasConsented());

  base::test::TestFuture<bool> opt_in_result;
  views::Widget* widget =
      ShowDialogAndWait(nullptr, opt_in_result.GetCallback());
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  // Verify Shown metrics
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Glic.ExperimentalTriggering.OptIn.Actuation.Shown"),
            1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.OptIn.Shown.Version",
      RequiredExperimentalOptIn::kActuation, 1);

  // Accept opt-in.
  ASSERT_TRUE(ExecJs(guest_contents, "window.location.hash = '#continue';"));

  views::test::WidgetDestroyedWaiter(widget).Wait();

  EXPECT_TRUE(opt_in_result.Get());

  // Verify Glic remains complete, and Actuation AND Experimental are enabled (2
  // opt-ins).
  EXPECT_TRUE(service()->enabling().HasConsented());
  EXPECT_TRUE(service()->enabling().GetUserEnabledActuationOnWeb());
  EXPECT_TRUE(service()->enabling().GetExperimentalTriggeringEnabled());

  // Verify Accept metrics
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Glic.ExperimentalTriggering.OptIn.Actuation.Accepted"),
            1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.OptIn.Accepted.Version",
      RequiredExperimentalOptIn::kActuation, 1);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, AcceptOptInExperimental) {
  base::UserActionTester user_action_tester;
  base::HistogramTester histogram_tester;

  // Set Glic complete, Actuation complete, but Experimental Triggering
  // incomplete.
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service()->enabling().SetUserEnabledActuationOnWeb(true);
  service()->enabling().SetExperimentalTriggeringEnabled(false);

  base::test::TestFuture<bool> opt_in_result;
  views::Widget* widget =
      ShowDialogAndWait(nullptr, opt_in_result.GetCallback());
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  // Verify Shown metrics
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Glic.ExperimentalTriggering.OptIn.Experimental.Shown"),
            1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.OptIn.Shown.Version",
      RequiredExperimentalOptIn::kExperimental, 1);

  // Accept opt-in.
  ASSERT_TRUE(ExecJs(guest_contents, "window.location.hash = '#continue';"));

  views::test::WidgetDestroyedWaiter(widget).Wait();

  EXPECT_TRUE(opt_in_result.Get());

  // Verify Glic is consented, Actuation remains enabled, and Experimental is
  // enabled (1 opt-in).
  EXPECT_TRUE(service()->enabling().HasConsented());
  EXPECT_TRUE(service()->enabling().GetUserEnabledActuationOnWeb());
  EXPECT_TRUE(service()->enabling().GetExperimentalTriggeringEnabled());

  // Verify Accept metrics
  EXPECT_EQ(user_action_tester.GetActionCount(
                "Glic.ExperimentalTriggering.OptIn.Experimental.Accepted"),
            1);
  histogram_tester.ExpectUniqueSample(
      "Glic.ExperimentalTriggering.OptIn.Accepted.Version",
      RequiredExperimentalOptIn::kExperimental, 1);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, MultipleOptInRequests) {
  auto* service =
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->GetProfile());
  service->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(service->enabling().HasConsented());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<bool> future(base::test::TestFutureMode::kQueue);

  service->opt_in_controller().ShowDialog(web_contents, future.GetCallback());
  views::Widget* widget1 =
      service->opt_in_controller().GetDialogWidgetForTesting();
  ASSERT_TRUE(widget1);

  // Second request should return the same widget and queue callbacks.
  service->opt_in_controller().ShowDialog(web_contents, future.GetCallback());
  views::Widget* widget2 =
      service->opt_in_controller().GetDialogWidgetForTesting();
  EXPECT_EQ(widget1, widget2);

  views::test::WidgetVisibleWaiter(widget1).Wait();
  EXPECT_TRUE(widget1->IsVisible());

  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  content::WebContents* guest_contents = guest_view->web_contents();
  ASSERT_TRUE(guest_contents);
  EXPECT_TRUE(content::WaitForLoadStop(guest_contents));

  // Accept opt-in.
  ASSERT_TRUE(ExecJs(guest_contents, "window.location.hash = '#continue';"));

  views::test::WidgetDestroyedWaiter(widget1).Wait();

  EXPECT_TRUE(future.Take());
  EXPECT_TRUE(future.Take());
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, SyncsCookiesToWebview) {
  Profile* profile = browser()->GetProfile();
  auto* service_ptr = GlicKeyedServiceFactory::GetGlicKeyedService(profile);

  signin::SetAutomaticIssueOfAccessTokens(
      IdentityManagerFactory::GetForProfile(profile), true);

  fake_gaia().SetupFakeGaiaForLoginWithDefaults();
  FakeGaia::Configuration config;
  config.emails = {"glic-test@example.com"};
  config.session_sid_cookie = FakeGaiaMixin::kFakeSIDCookie;
  config.session_lsid_cookie = FakeGaiaMixin::kFakeLSIDCookie;
  fake_gaia().fake_gaia()->UpdateConfiguration(config);

  // Open the opt-in dialog. Real cookie synchronization will take place.
  service_ptr->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service_ptr->enabling().SetUserEnabledActuationOnWeb(true);
  service_ptr->enabling().SetExperimentalTriggeringEnabled(false);

  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      switches::kGlicExperimentalFreURL,
      embedded_https_test_server().GetURL("google.fr", "/title1.html").spec());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  views::Widget* widget = ShowDialogAndWait(web_contents);
  ASSERT_TRUE(widget);

  // Wait for the guest webview to load successfully.
  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  // Confirm directly within the webview DOM that the Google cookie is
  // accessible. Note that FakeGaia hardcodes ".google.fr" for multilogin
  // cookies.
  std::string webview_cookies =
      EvalJs(guest_contents, "document.cookie").ExtractString();
  EXPECT_NE(
      webview_cookies.find(std::string("SID=") + FakeGaiaMixin::kFakeSIDCookie),
      std::string::npos)
      << "The webview DOM failed to read the synced Google cookie! "
         "document.cookie: "
      << webview_cookies;

  service_ptr->opt_in_controller().CloseDialog(false);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, NoAccountCookieSyncFails) {
  base::UserActionTester tester;
  Profile* profile = browser()->GetProfile();

  signin::SetAutomaticIssueOfAccessTokens(
      IdentityManagerFactory::GetForProfile(profile), false);

  // Invalidate primary account credentials so that prod GlicCookieSynchronizer
  // definitively fails to fetch a token.
  InvalidateAccount(profile);

  auto* service_ptr = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  service_ptr->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service_ptr->enabling().SetUserEnabledActuationOnWeb(true);
  service_ptr->enabling().SetExperimentalTriggeringEnabled(false);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Capture the dialog result to verify it gets rejected upon closing.
  base::test::TestFuture<bool> opt_in_result;
  views::Widget* widget =
      ShowDialogAndWait(web_contents, opt_in_result.GetCallback());
  ASSERT_TRUE(widget);

  content::WebContents* dialog_contents = GetDialogWebContents();
  ASSERT_TRUE(dialog_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dialog_contents));

  // Shown metrics for experimental modal are recorded synchronously on WebUI
  // creation.
  EXPECT_EQ(tester.GetActionCount(
                "Glic.ExperimentalTriggering.OptIn.Experimental.Shown"),
            1);

  // Wait for the async cookie sync to fail and the error panel to become
  // visible.
  bool error_panel_visible = base::test::RunUntil([this, dialog_contents]() {
    return EvalJs(dialog_contents, "isVisible(getErrorPanel())").ExtractBool();
  });
  ASSERT_TRUE(error_panel_visible);

  // Because cookie sync fails, the webview src is never set.
  EXPECT_EQ(false,
            EvalJs(dialog_contents, "!!getWebview()?.hasAttribute('src')"));
  EXPECT_EQ(0u, GetGuestViewManager()->num_guests_created());
  EXPECT_EQ(tester.GetActionCount("Glic.Onboarding.OptInImpression"), 0);

  // Verify the Try Again button is present in the DOM.
  EXPECT_EQ(true, EvalJs(dialog_contents, "!!getTryAgainButton()"));

  // Click the close button inside the error panel to dismiss the dialog.
  views::test::WidgetDestroyedWaiter waiter(widget);
  content::ExecuteScriptAsync(
      dialog_contents,
      std::string(kCommonJs) + "getCloseButtonError().click();");
  waiter.Wait();

  EXPECT_FALSE(opt_in_result.Get());
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, OpenGoogleLinkInNewTab) {
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);

  // Open a google link.
  ASSERT_TRUE(ExecJs(guest_contents,
                     "window.open('https://policies.google.com/', '_blank');"));

  // Wait for the new tab to be created.
  bool tab_created = base::test::RunUntil(
      [this]() { return browser()->tab_strip_model()->count() == 2; });
  EXPECT_TRUE(tab_created);
  EXPECT_EQ(browser()->tab_strip_model()->GetWebContentsAt(1)->GetVisibleURL(),
            GURL("https://policies.google.com/"));
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            browser()->tab_strip_model()->GetWebContentsAt(1));

  service()->opt_in_controller().CloseDialog(false);
}
class GlicExperimentalOptInOfflineTest : public GlicExperimentalOptInTest {
 public:
  GlicExperimentalOptInOfflineTest()
      : GlicExperimentalOptInTest(GURL("https://invalid.test/")) {}
};

// Regression test for b/516601993: Prevent webview from navigating to different
// origin.
IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       BlocksNavigationToOtherOrigin) {
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  GURL initial_url = guest_contents->GetLastCommittedURL();

  GURL disallowed_url =
      embedded_https_test_server().GetURL("b.test", "/title1.html");
  content::TestNavigationObserver nav_observer(guest_contents);
  ASSERT_TRUE(ExecJs(guest_contents, "window.location.href = '" +
                                         disallowed_url.spec() + "';"));

  nav_observer.Wait();

  EXPECT_EQ(guest_contents->GetLastCommittedURL(), initial_url);
  EXPECT_NE(guest_contents->GetLastCommittedURL(), disallowed_url);

  service()->opt_in_controller().CloseDialog(false);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInOfflineTest,
                       OfflinePageLoadFailureAndDismissal) {
  base::test::TestFuture<bool> opt_in_result;
  views::Widget* widget =
      ShowDialogAndWait(nullptr, opt_in_result.GetCallback());
  ASSERT_TRUE(widget);

  content::WebContents* dialog_contents = GetDialogWebContents();
  ASSERT_TRUE(dialog_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dialog_contents));

  // Wait for the WebUI host page to load and trigger the offline state.
  // The error panel should be visible (displaying the offline failure layout),
  // and the webview guest should be hidden.
  bool offline_panel_visible = base::test::RunUntil([this, dialog_contents]() {
    return EvalJs(dialog_contents, "isVisible(getErrorPanel())").ExtractBool();
  });

  ASSERT_TRUE(offline_panel_visible);

  // Verify that the close button is visible and has positive dimensions.
  bool close_button_visible =
      EvalJs(dialog_contents,
             "(() => {"
             "  const btn = getCloseButtonError();"
             "  const rect = btn.getBoundingClientRect();"
             "  return isVisible(btn) && rect.width > 0 && rect.height > 0;"
             "})()")
          .ExtractBool();
  EXPECT_TRUE(close_button_visible);

  // Click the close button inside the WebUI.
  views::test::WidgetDestroyedWaiter waiter(widget);
  content::ExecuteScriptAsync(
      dialog_contents,
      std::string(kCommonJs) + "getCloseButtonError().click();");

  // The dialog should close and the result should be false (rejected).
  waiter.Wait();
  EXPECT_FALSE(opt_in_result.Get());
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInOfflineTest,
                       OfflinePageLoadFailureAndRetry) {
  base::test::TestFuture<bool> opt_in_result;
  views::Widget* widget =
      ShowDialogAndWait(nullptr, opt_in_result.GetCallback());
  ASSERT_TRUE(widget);

  content::WebContents* dialog_contents = GetDialogWebContents();
  ASSERT_TRUE(dialog_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dialog_contents));

  // 1. Wait for the WebUI host page to load and trigger the offline state.
  bool offline_panel_visible = base::test::RunUntil([this, dialog_contents]() {
    return EvalJs(dialog_contents, "isVisible(getErrorPanel())").ExtractBool();
  });
  ASSERT_TRUE(offline_panel_visible);

  // Wait for the guest view to be created for the initial failed attempt.
  content::WebContents* guest_contents = WaitForGuestContents(false);
  ASSERT_TRUE(guest_contents);

  // 2. Intercept the retry request to "invalid.test" and make it succeed.
  // Because we instantiate this AFTER the first failure, we don't need a
  // counter. We just unconditionally return a mock success response.
  content::URLLoaderInterceptor interceptor(base::BindRepeating(
      [](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.host() == "invalid.test") {
          std::string html = "<html><body>Success!</body></html>";
          content::URLLoaderInterceptor::WriteResponse(
              "HTTP/1.1 200 OK\nContent-type: text/html\n\n", html,
              params->client.get());
          return true;
        }
        return false;
      }));

  // Set up a navigation observer to wait for the retry navigation to complete.
  content::TestNavigationObserver navigation_observer(guest_contents);

  // 3. Click the "Try Again" button inside the WebUI error panel.
  bool try_again_button_clicked =
      ExecJs(dialog_contents, "getTryAgainButton().click();");
  ASSERT_TRUE(try_again_button_clicked);

  // 4. Wait for the navigation to complete and verify success.
  navigation_observer.Wait();
  EXPECT_TRUE(navigation_observer.last_navigation_succeeded());

  // Verify the error panel hides and the webview becomes visible.
  bool error_panel_hidden = base::test::RunUntil([this, dialog_contents]() {
    return EvalJs(dialog_contents, "isHidden(getErrorPanel())").ExtractBool();
  });
  EXPECT_TRUE(error_panel_hidden);
  EXPECT_EQ(false, EvalJs(dialog_contents, "!!getWebview().hidden"));

  // Clean up by closing the dialog.
  views::test::WidgetDestroyedWaiter waiter(widget);
  service()->opt_in_controller().CloseDialog(false);
  waiter.Wait();
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       GetOrCreateSuitableWebContents_NoTriggeringTab) {
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);

  GURL fallback_url(
      features::kGlicExperimentalTriggeringTabFocusFallbackURL.Get());

  content::WebContents* web_contents =
      service()->opt_in_controller().GetOrCreateSuitableWebContents();
  ASSERT_TRUE(web_contents);

  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);
  // The new tab should be opened in the background.
  EXPECT_NE(browser()->tab_strip_model()->GetActiveWebContents(), web_contents);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);
  EXPECT_EQ(web_contents->GetURL(), fallback_url);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest,
                       GetOrCreateSuitableWebContents_ExistingTriggeringTab) {
  GURL fallback_url(
      features::kGlicExperimentalTriggeringTabFocusFallbackURL.Get());

  chrome::AddSelectedTabWithURL(browser(), fallback_url,
                                ui::PAGE_TRANSITION_LINK);
  content::WebContents* triggering_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TestNavigationObserver observer(triggering_contents);
  observer.Wait();
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  // Move target tab to the background.
  browser()->tab_strip_model()->ActivateTabAt(0);

  // Confirm we get the target background tab's web contents...
  content::WebContents* web_contents =
      service()->opt_in_controller().GetOrCreateSuitableWebContents();
  ASSERT_TRUE(web_contents);
  EXPECT_EQ(web_contents, triggering_contents);

  // ... but that it is not yet activated.
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 0);
  EXPECT_EQ(browser()->tab_strip_model()->count(), 2);

  // Showing the dialog should activate the tab.
  views::Widget* widget = ShowDialogAndWait(triggering_contents);
  ASSERT_TRUE(widget);
  EXPECT_EQ(browser()->tab_strip_model()->GetActiveWebContents(),
            triggering_contents);
  EXPECT_EQ(browser()->tab_strip_model()->active_index(), 1);
}

IN_PROC_BROWSER_TEST_F(
    GlicExperimentalOptInTest,
    GetOrCreateSuitableWebContents_TargetInBackgroundWindow) {
  // 1. We start with browser() (Window A, active).
  // It has 1 tab (not matching).
  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);

  // 2. Create Window B (background window).
  BrowserWindowInterface* window_b = CreateAdditionalBrowserWindow();

  // Ensure Window A is active.
  browser()->GetWindow()->Activate();
  ASSERT_OK(WaitForWindowActive(browser()));

  // 3. Add suitable tab to Window B.
  GURL fallback_url(
      features::kGlicExperimentalTriggeringTabFocusFallbackURL.Get());
  content::WebContents* triggering_contents = chrome::AddAndReturnTabAt(
      window_b, fallback_url, -1, /*foreground=*/true);
  ASSERT_TRUE(triggering_contents);
  content::TestNavigationObserver observer(triggering_contents);
  observer.Wait();
  // Ensure Window A is still active overall.
  browser()->GetWindow()->Activate();
  ASSERT_OK(WaitForWindowActive(browser()));

  // 4. Call GetOrCreateSuitableWebContents.
  content::WebContents* web_contents =
      service()->opt_in_controller().GetOrCreateSuitableWebContents();
  ASSERT_TRUE(web_contents);

  // It should find the tab in Window B.
  EXPECT_EQ(web_contents, triggering_contents);

  // Window A should still be active overall.
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser(),
            browser());

  // 5. Show dialog on the contents in Window B.
  views::Widget* widget = ShowDialogAndWait(web_contents);
  ASSERT_TRUE(widget);

  // Window B should now be active!
  EXPECT_EQ(GlobalBrowserCollection::GetInstance()->GetLastActiveBrowser(),
            window_b);
}

}  // namespace glic
