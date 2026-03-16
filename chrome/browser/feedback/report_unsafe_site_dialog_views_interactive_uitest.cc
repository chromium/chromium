// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback.h"
#include "base/test/mock_callback.h"
#include "base/test/scoped_feature_list.h"
#include "build/branding_buildflags.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/report_unsafe_site_dialog.h"
#include "chrome/browser/feedback/report_unsafe_site_dialog_views.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/ui_test_utils.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/content/browser/client_side_detection_host.h"
#include "components/safe_browsing/content/browser/safe_browsing_tab_observer.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "components/url_formatter/elide_url.h"
#include "content/public/test/browser_test.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/views/interaction/interactive_views_test.h"

namespace {

DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDialogWebviewId);

using safe_browsing::ClientSideDetectionHost;
using ::testing::_;

}  // anonymous namespace

// Interactive UI tests for report-unsafe-site dialog.
class ReportUnsafeSiteDialogInteractiveUiTest : public InteractiveBrowserTest {
 public:
  ReportUnsafeSiteDialogInteractiveUiTest() = default;
  ~ReportUnsafeSiteDialogInteractiveUiTest() override = default;

  void SetUp() override {
    embedded_https_test_server().SetCertHostnames({"example.com"});
    embedded_https_test_server().ServeFilesFromSourceDirectory(
        GetChromeTestDataDir());
    ASSERT_TRUE(embedded_https_test_server().Start());
    InteractiveBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    InteractiveBrowserTest::SetUpCommandLine(command_line);
    // Set host resolver rules so that https://example.com/ redirects to the
    // test server. This avoids having a port number in the page URL and
    // keeps the page URL constant for screenshots.
    command_line->AppendSwitchASCII(
        ::network::switches::kHostResolverRules,
        base::StringPrintf(
            "MAP * %s",
            embedded_https_test_server().host_port_pair().ToString().c_str()));
  }

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(embedded_test_server()->Start());

    PrefService* prefs = GetProfile()->GetPrefs();
    prefs->SetBoolean(prefs::kUserFeedbackAllowed, true);
    safe_browsing::SetSafeBrowsingState(
        prefs, safe_browsing::SafeBrowsingState::STANDARD_PROTECTION);
  }

  // Show dialog via browser command in order to show dialog on non-branded
  // builds.
  auto ExecuteReportUnsafeSiteCommand() {
    return WithView(kBrowserViewElementId, [](BrowserView* browser_view) {
      feedback::ReportUnsafeSiteDialog::Show(browser_view->browser());
    });
  }

  auto ClickDialogElement(const std::string& element_selector) {
    return InAnyContext(ClickElement(
        kDialogWebviewId, {"report-unsafe-site-app", element_selector},
        ui_controls::LEFT, ui_controls::kNoAccelerator,
        ExecuteJsMode::kFireAndForget));
  }

  auto WaitForDialog() {
    return InAnyContext(
        WaitForShow(
            feedback::ReportUnsafeSiteDialogViews::kReportUnsafeSiteDialogId),
        InstrumentNonTabWebView(kDialogWebviewId,
                                feedback::kReportUnsafeSiteWebviewElementId));
  }

  auto WaitForDialogHide() {
    return InAnyContext(WaitForHide(
        feedback::ReportUnsafeSiteDialogViews::kReportUnsafeSiteDialogId));
  }

  auto CloseDialog() {
    return InAnyContext(ClickDialogElement(".cancel-button"),
                        WaitForDialogHide());
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_{
      features::kReportUnsafeSite};
};

#if BUILDFLAG(GOOGLE_CHROME_BRANDING)
IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogInteractiveUiTest,
                       ShowDialogViaAppMenu) {
  RunTestSequence(PressButton(kToolbarAppMenuButtonElementId),
                  SelectMenuItem(AppMenuModel::kHelpMenuItem),
                  SelectMenuItem(HelpMenuModel::kReportUnsafeSiteMenuItem),
                  WaitForDialog(), CloseDialog());
}

// Only branded builds have links in dialog.
IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogInteractiveUiTest,
                       ClickOnUnsafeSitePolicy) {
  const GURL kExpectedLinkUrl("https://safebrowsing.google.com/#policies");
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents2Id);
  RunTestSequence(
      ExecuteReportUnsafeSiteCommand(), WaitForDialog(),
      InAnyContext(
          ClickElement(kDialogWebviewId,
                       {"report-unsafe-site-app", "#unsafe_site_policy_link"},
                       ui_controls::LEFT, ui_controls::kNoAccelerator,
                       ExecuteJsMode::kFireAndForget)),
      InstrumentNextTab(kWebContents2Id),
      // Use WaitForWebContentsReady() to check URL of new tab.
      WaitForWebContentsReady(kWebContents2Id, kExpectedLinkUrl),
      // Opening the new tab should have hidden the tab-modal dialog.
      WaitForHide(
          feedback::ReportUnsafeSiteDialogViews::kReportUnsafeSiteDialogId));
}
#endif  // BUILDFLAG(GOOGLE_CHROME_BRANDING)

IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogInteractiveUiTest, ShowDialog) {
  RunTestSequence(ExecuteReportUnsafeSiteCommand(), WaitForDialog(),
                  CloseDialog());
}

IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogInteractiveUiTest, UrlInDialog) {
  GURL url = embedded_test_server()->GetURL("/simple.html");
  std::u16string formatted_origin =
      url_formatter::FormatUrlForSecurityDisplay(url);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  RunTestSequence(
      ExecuteReportUnsafeSiteCommand(), WaitForDialog(),
      WaitForJsResultAt(
          kDialogWebviewId,
          {"report-unsafe-site-app", ".url-input-container", "input"},
          "(el) => el.value", ::testing::Eq(formatted_origin)),
      CloseDialog());
}

// Pixel test for report-unsafe-site dialog when the user opts to not send the
// webpage screenshot to Google.
IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogInteractiveUiTest,
                       ExcludeScreenshotPixelTest) {
  const GURL kTestUrl("https://example.com/simple.html");
  std::u16string formatted_origin =
      url_formatter::FormatUrlForSecurityDisplay(kTestUrl);

  BrowserView::GetBrowserViewForBrowser(browser())->GetWidget()->SetSize(
      {800, 800});

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), kTestUrl));
  RunTestSequence(
      SetOnIncompatibleAction(
          OnIncompatibleAction::kIgnoreAndContinue,
          "Screenshots not supported in all testing environments."),
      ExecuteReportUnsafeSiteCommand(), WaitForDialog(),
      WaitForJsResultAt(
          kDialogWebviewId,
          {"report-unsafe-site-app", ".url-input-container", "input"},
          "(el) => el.value", ::testing::Eq(formatted_origin)),
      CheckJsResultAt(kDialogWebviewId,
                      {"report-unsafe-site-app", "#includeScreenshotCheckbox"},
                      "(el) => el.checked", ::testing::Eq(true)),
      ClickDialogElement("#includeScreenshotCheckbox"),
      WaitForJsResultAt(
          kDialogWebviewId,
          {"report-unsafe-site-app", "#includeScreenshotCheckbox"},
          "(el) => el.checked", ::testing::Eq(false)),
      InAnyContext(
          FocusElement(kDialogWebviewId),
          ScreenshotSurface(kDialogWebviewId,
                            /*screenshot_name=*/"ReportUnsafeSiteDialog",
                            /*baseline_cl=*/"7650257")),
      CloseDialog());
}

IN_PROC_BROWSER_TEST_F(ReportUnsafeSiteDialogInteractiveUiTest, SendReport) {
  base::MockCallback<ClientSideDetectionHost::PreclassificationStarted>
      preclassification_started_callback;
  EXPECT_CALL(preclassification_started_callback, Run(_))
      .Times(testing::AnyNumber());
  EXPECT_CALL(preclassification_started_callback,
              Run(safe_browsing::ClientSideDetectionType::USER_REPORT));

  GURL url = embedded_test_server()->GetURL("/simple.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  content::WebContents* const web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  auto* client_side_detection_host =
      safe_browsing::SafeBrowsingTabObserver::FromWebContents(web_contents)
          ->client_side_detection_host();

  base::ScopedClosureRunner cleanup(
      base::BindOnce(&safe_browsing::ClientSideDetectionHost::
                         set_preclassification_started_callback_for_testing,
                     base::Unretained(client_side_detection_host),
                     ClientSideDetectionHost::PreclassificationStarted()));

  client_side_detection_host
      ->set_preclassification_started_callback_for_testing(
          preclassification_started_callback.Get());
  RunTestSequence(
      ExecuteReportUnsafeSiteCommand(), WaitForDialog(),
      WaitForJsResultAt(kDialogWebviewId,
                        {"report-unsafe-site-app", ".action-button"},
                        "(el) => el.disabled", ::testing::Eq(false)),
      ClickDialogElement(".action-button"), WaitForDialogHide());
}
