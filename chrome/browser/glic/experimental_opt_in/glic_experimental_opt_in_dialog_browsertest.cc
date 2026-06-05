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
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/webui_url_constants.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/guest_view/browser/guest_view_base.h"
#include "components/guest_view/browser/guest_view_manager_delegate.h"
#include "components/guest_view/browser/test_guest_view_manager.h"
#include "components/signin/public/identity_manager/identity_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/interaction/element_tracker_views.h"
#include "ui/views/test/widget_test.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/widget.h"

namespace glic {

class GlicExperimentalOptInTest
    : public GlicBrowserTestMixin<MixinBasedInProcessBrowserTest> {
 public:
  using BaseClass = GlicBrowserTestMixin<MixinBasedInProcessBrowserTest>;
  using MixinBasedInProcessBrowserTest::browser;
  GlicExperimentalOptInTest() = default;
  ~GlicExperimentalOptInTest() override = default;

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
    GURL test_url =
        opt_in_test_server_.GetURL("a.test", "/test_data/page.html");

    base::FieldTrialParams params;
    params["glic-experimental-triggering-opt-in-url"] = test_url.spec();
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kGlicExperimentalTriggering, params);

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
        IdentityManagerFactory::GetForProfile(browser()->profile()), true);
  }

  guest_view::TestGuestViewManager* GetGuestViewManager() {
    return guest_view_manager_factory_.GetOrCreateTestGuestViewManager(
        browser()->profile(), extensions::ExtensionsAPIClient::Get()
                                  ->CreateGuestViewManagerDelegate());
  }

  views::Widget* ShowDialogAndWait(
      content::WebContents* web_contents = nullptr,
      base::OnceCallback<void(bool)> callback = base::DoNothing()) {
    if (!web_contents) {
      web_contents = browser()->tab_strip_model()->GetActiveWebContents();
    }
    views::Widget* widget = service()->opt_in_controller().ShowDialog(
        web_contents, std::move(callback));
    if (!widget) {
      return nullptr;
    }
    views::test::WidgetVisibleWaiter(widget).Wait();
    EXPECT_TRUE(widget->IsVisible());
    return widget;
  }

  content::WebContents* WaitForGuestContents() {
    auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
    if (!guest_view) {
      return nullptr;
    }
    content::WebContents* guest_contents = guest_view->web_contents();
    if (!guest_contents) {
      return nullptr;
    }
    EXPECT_TRUE(content::WaitForLoadStop(guest_contents));
    return guest_contents;
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
    expected_url = DecorateGlicOptInUrl(browser()->profile(), expected_url);
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

  base::test::ScopedFeatureList feature_list_;
  base::CallbackListSubscription creation_subscription_;
  net::EmbeddedTestServer opt_in_test_server_;
  guest_view::TestGuestViewManagerFactory guest_view_manager_factory_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, OpensDialog) {
  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  GlicExperimentalOptInDialogView* dialog_view =
      service()->opt_in_controller().GetDialogViewForTesting();
  ASSERT_TRUE(dialog_view);
  views::WebView* web_view = dialog_view->GetWebViewForTesting();
  ASSERT_TRUE(web_view);
  content::WebContents* dialog_contents = web_view->GetWebContents();
  ASSERT_TRUE(dialog_contents);

  EXPECT_TRUE(content::WaitForLoadStop(dialog_contents));
  ASSERT_TRUE(dialog_contents->GetController().GetLastCommittedEntry());
  EXPECT_EQ(
      dialog_contents->GetController().GetLastCommittedEntry()->GetPageType(),
      content::PAGE_TYPE_NORMAL);
  EXPECT_EQ(dialog_contents->GetLastCommittedURL(),
            GURL(chrome::kChromeUIGlicExperimentalOptInURL));

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
  views::Widget* widget = service()->opt_in_controller().ShowDialog(
      web_contents, future.GetCallback());
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

  GlicExperimentalOptInDialogView* dialog_view =
      service()->opt_in_controller().GetDialogViewForTesting();
  ASSERT_TRUE(dialog_view);
  views::WebView* web_view = dialog_view->GetWebViewForTesting();
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

  GlicExperimentalOptInDialogView* dialog_view =
      service()->opt_in_controller().GetDialogViewForTesting();
  ASSERT_TRUE(dialog_view);
  views::WebView* web_view = dialog_view->GetWebViewForTesting();
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

  GlicExperimentalOptInDialogView* dialog_view =
      service()->opt_in_controller().GetDialogViewForTesting();
  ASSERT_TRUE(dialog_view);
  views::WebView* web_view = dialog_view->GetWebViewForTesting();
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
  ASSERT_TRUE(
      content::ExecJs(guest_contents, "window.location.hash = '#continue';"));

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
  ASSERT_TRUE(
      content::ExecJs(guest_contents, "window.location.hash = '#noThanks';"));

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
  ASSERT_TRUE(
      content::ExecJs(guest_contents, "window.location.hash = '#continue';"));

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
  ASSERT_TRUE(
      content::ExecJs(guest_contents, "window.location.hash = '#continue';"));

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
      GlicKeyedServiceFactory::GetGlicKeyedService(browser()->profile());
  service->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  ASSERT_FALSE(service->enabling().HasConsented());

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  base::test::TestFuture<bool> future(base::test::TestFutureMode::kQueue);

  views::Widget* widget1 = service->opt_in_controller().ShowDialog(
      web_contents, future.GetCallback());
  ASSERT_TRUE(widget1);

  // Second request should return the same widget and queue callbacks.
  views::Widget* widget2 = service->opt_in_controller().ShowDialog(
      web_contents, future.GetCallback());
  EXPECT_EQ(widget1, widget2);

  views::test::WidgetVisibleWaiter(widget1).Wait();
  EXPECT_TRUE(widget1->IsVisible());

  auto* guest_view = GetGuestViewManager()->WaitForSingleGuestViewCreated();
  ASSERT_TRUE(guest_view);
  content::WebContents* guest_contents = guest_view->web_contents();
  ASSERT_TRUE(guest_contents);
  EXPECT_TRUE(content::WaitForLoadStop(guest_contents));

  // Accept opt-in.
  ASSERT_TRUE(
      content::ExecJs(guest_contents, "window.location.hash = '#continue';"));

  views::test::WidgetDestroyedWaiter(widget1).Wait();

  EXPECT_TRUE(future.Take());
  EXPECT_TRUE(future.Take());
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, SyncsCookiesToWebview) {
  Profile* profile = browser()->profile();
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
      content::EvalJs(guest_contents, "document.cookie").ExtractString();
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
  Profile* profile = browser()->profile();

  // Invalidate primary account credentials so that prod GlicCookieSynchronizer
  // fails.
  InvalidateAccount(profile);

  auto* service_ptr = GlicKeyedServiceFactory::GetGlicKeyedService(profile);
  service_ptr->enabling().SetCompletedFre(glic::prefs::FreStatus::kCompleted);
  service_ptr->enabling().SetUserEnabledActuationOnWeb(true);
  service_ptr->enabling().SetExperimentalTriggeringEnabled(false);

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  views::Widget* widget = ShowDialogAndWait(web_contents);
  ASSERT_TRUE(widget);

  GlicExperimentalOptInDialogView* dialog_view =
      service_ptr->opt_in_controller().GetDialogViewForTesting();
  ASSERT_TRUE(dialog_view);
  views::WebView* web_view = dialog_view->GetWebViewForTesting();
  ASSERT_TRUE(web_view);
  content::WebContents* dialog_contents = web_view->GetWebContents();
  ASSERT_TRUE(dialog_contents);
  EXPECT_TRUE(content::WaitForLoadStop(dialog_contents));

  // Shown metrics for experimental modal are recorded synchronously on WebUI
  // creation.
  EXPECT_EQ(tester.GetActionCount(
                "Glic.ExperimentalTriggering.OptIn.Experimental.Shown"),
            1);

  // Because cookie sync fails, the webview src is never set. EvalJs
  // synchronously verifies JS execution completed without setting src.
  // Therefore, no guest webview loads, ensuring 0 impressions non-racily.
  EXPECT_EQ(false,
            content::EvalJs(
                dialog_contents,
                "!!document.querySelector('webview')?.hasAttribute('src')"));
  EXPECT_EQ(0u, GetGuestViewManager()->num_guests_created());
  EXPECT_EQ(tester.GetActionCount("Glic.Onboarding.OptInImpression"), 0);

  service_ptr->opt_in_controller().CloseDialog(false);
}

IN_PROC_BROWSER_TEST_F(GlicExperimentalOptInTest, OpenGoogleLinkInNewTab) {
  service()->enabling().SetCompletedFre(glic::prefs::FreStatus::kIncomplete);
  views::Widget* widget = ShowDialogAndWait();
  ASSERT_TRUE(widget);

  content::WebContents* guest_contents = WaitForGuestContents();
  ASSERT_TRUE(guest_contents);

  EXPECT_EQ(browser()->tab_strip_model()->count(), 1);

  // Open a google link.
  ASSERT_TRUE(content::ExecJs(
      guest_contents,
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
  ASSERT_TRUE(content::ExecJs(
      guest_contents,
      "window.location.href = '" + disallowed_url.spec() + "';"));

  nav_observer.Wait();

  EXPECT_EQ(guest_contents->GetLastCommittedURL(), initial_url);
  EXPECT_NE(guest_contents->GetLastCommittedURL(), disallowed_url);

  service()->opt_in_controller().CloseDialog(false);
}

}  // namespace glic
