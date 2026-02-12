// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_features.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/feedback/report_unsafe_site_dialog.h"
#include "chrome/browser/feedback/report_unsafe_site_dialog_views.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/toolbar/app_menu_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/interaction/interactive_browser_test.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing/core/common/safe_browsing_prefs.h"
#include "content/public/test/browser_test.h"
#include "ui/base/interaction/element_identifier.h"
#include "ui/base/interaction/interactive_test.h"
#include "ui/views/interaction/interactive_views_test.h"

// Interactive UI tests for report-unsafe-site dialog.
class ReportUnsafeSiteDialogInteractiveUiTest : public InteractiveBrowserTest {
 public:
  ReportUnsafeSiteDialogInteractiveUiTest() = default;
  ~ReportUnsafeSiteDialogInteractiveUiTest() override = default;

  void SetUpOnMainThread() override {
    InteractiveBrowserTest::SetUpOnMainThread();
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

  auto WaitForDialog() {
    return InAnyContext(WaitForShow(
        feedback::ReportUnsafeSiteDialogViews::kReportUnsafeSiteDialogId));
  }

  auto CloseDialog() {
    return InAnyContext(
        PressButton(views::BubbleFrameView::kCloseButtonElementId),
        WaitForHide(
            feedback::ReportUnsafeSiteDialogViews::kReportUnsafeSiteDialogId));
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
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kDialogWebviewId);
  DEFINE_LOCAL_ELEMENT_IDENTIFIER_VALUE(kWebContents2Id);
  RunTestSequence(
      ExecuteReportUnsafeSiteCommand(), WaitForDialog(),
      InAnyContext(
          InstrumentNonTabWebView(kDialogWebviewId,
                                  feedback::kReportUnsafeSiteWebviewElementId),
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
