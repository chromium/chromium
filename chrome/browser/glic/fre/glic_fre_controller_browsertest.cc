// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/glic_fre_controller.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/run_until.h"
#include "base/time/time.h"
#include "chrome/browser/glic/fre/glic_fre_page_handler.h"
#include "chrome/browser/glic/glic_pref_names.h"
#include "chrome/browser/glic/public/glic_keyed_service.h"
#include "chrome/browser/glic/test_support/glic_test_environment.h"
#include "chrome/browser/glic/test_support/non_interactive_glic_test.h"
#include "chrome/browser/lifetime/application_lifetime_desktop.h"
#include "chrome/browser/signin/identity_test_environment_profile_adaptor.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/views/widget/widget.h"

namespace glic {
namespace {

// Fake handler to allow testing multi-handler coordination.
class FakeGlicFrePageHandler : public GlicFrePageHandler {
 public:
  FakeGlicFrePageHandler(bool is_unified,
                         content::WebContents* web_contents,
                         GlicFreController& controller)
      : GlicFrePageHandler(is_unified, web_contents, {}) {}

  void SimulateAccept() { AcceptFre(); }
};

class GlicFreControllerBrowserTest : public NonInteractiveGlicTest {
 public:
  GlicFreControllerBrowserTest()
      : NonInteractiveGlicTest(
            {},
            GlicTestEnvironmentConfig{.fre_status =
                                          prefs::FreStatus::kNotStarted}) {}
  ~GlicFreControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    NonInteractiveGlicTest::SetUpOnMainThread();
  }

  GlicFreController& glic_fre_controller() {
    return glic_service()->fre_controller();
  }

  tabs::TabInterface* GetTabInterfaceForActiveWebContents(Browser* browser) {
    content::WebContents* tab_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    return tabs::TabInterface::MaybeGetFromContents(tab_web_contents);
  }

  void WaitForFreShow() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return glic_fre_controller().IsShowingDialog();
    })) << "FRE dialog should have been shown";
  }

  void WaitForFreInitialized() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return glic_fre_controller().IsShowingDialogAndStateInitialized();
    })) << "FRE dialog should have been initialized";
  }

  void WaitForFreClose() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return !glic_fre_controller().IsShowingDialog();
    })) << "FRE dialog should have been closed";
  }

  void WaitForGlicPanelShow() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return glic_service()->IsWindowShowing();
    })) << "Glic panel should have been shown";
  }

  void EnsureFreDoesNotShow() {
    auto end_time = base::TimeTicks::Now() + base::Milliseconds(500);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return end_time < base::TimeTicks::Now(); }));
    ASSERT_FALSE(glic_fre_controller().IsShowingDialog())
        << "FRE dialog should not have been shown";
  }

 protected:
  base::UserActionTester user_action_tester_;
  base::HistogramTester histogram_tester_;
};

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreDialogShowBlockedByModalUI) {
  tabs::TabInterface* tab = GetTabInterfaceForActiveWebContents(browser());

  // FRE dialog should be blocked from showing if another modal dialog is
  // already open.
  auto scoped_tab_modal_ui = tab->ShowModalUI();
  EXPECT_FALSE(glic_fre_controller().CanShowFreDialog(browser()));

  // The FRE dialog should be able to open after the existing modal dialog
  // is closed.
  scoped_tab_modal_ui.reset();
  EXPECT_TRUE(glic_fre_controller().CanShowFreDialog(browser()));
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);

  // Verify the FRE dialog is shown.
  WaitForFreShow();
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreDialogBlocksOtherModalUI) {
  tabs::TabInterface* tab = GetTabInterfaceForActiveWebContents(browser());

  // The FRE dialog should be able to open with no other modal dialogs open.
  EXPECT_TRUE(glic_fre_controller().CanShowFreDialog(browser()));
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);

  // Verify the FRE dialog is shown.
  WaitForFreShow();

  // Verify that another modal dialog cannot be shown now that the FRE is open.
  EXPECT_FALSE(tab->CanShowModalUI());

  // Once the FRE is closed, other modal dialogs can be shown again.
  glic_fre_controller().DismissFre(mojom::FreWebUiState::kReady);
  EXPECT_TRUE(tab->CanShowModalUI());
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreDialogShowBlockedByItself) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(glic_fre_controller().CanShowFreDialog(browser()));
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreShow();

  // Showing the FRE should be blocked as it is already open in the same tab.
  EXPECT_FALSE(glic_fre_controller().CanShowFreDialog(browser()));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       DismissFreDialogOnActiveTab) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreShow();

  // Close the FRE on the active tab.
  glic_fre_controller().DismissFreIfOpenOnActiveTab(browser());
  EXPECT_FALSE(glic_fre_controller().IsShowingDialog());
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       ShowFreDialogOnFailedCookieSync) {
  glic_test_service().SetResultForFutureCookieSyncInFre(false);
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreShow();
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       DontDismissFreDialogOnInactiveTab) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreShow();

  // Open a new tab at the end of the tab strip and activate it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(1);

  // Attempting to close the FRE on the active tab should do nothing.
  glic_fre_controller().DismissFreIfOpenOnActiveTab(browser());
  EXPECT_TRUE(glic_fre_controller().IsShowingDialog());
}

// TODO(crbug.com/402310277): Re-enable this test.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
#define MAYBE_FreDialogCloseAndReopenForDifferentTab \
  DISABLED_FreDialogCloseAndReopenForDifferentTab
#else
#define MAYBE_FreDialogCloseAndReopenForDifferentTab \
  FreDialogCloseAndReopenForDifferentTab
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
// Tests that, when the FRE dialog is already open in an inactive tab, trying to
// show it in the active tab closes the existing dialog and opens a new one.
IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       MAYBE_FreDialogCloseAndReopenForDifferentTab) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreShow();
  tabs::TabInterface* original_tab =
      GetTabInterfaceForActiveWebContents(browser());

  // Open a new tab at the end of the tab strip and activate it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(1);
  tabs::TabInterface* new_tab = GetTabInterfaceForActiveWebContents(browser());

  // Opening the FRE dialog should close the existing dialog.
  EXPECT_TRUE(glic_fre_controller().CanShowFreDialog(browser()));
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreShow();
  // The original tab no longer has a modal, while the active one does.
  EXPECT_TRUE(original_tab->CanShowModalUI());
  EXPECT_FALSE(new_tab->CanShowModalUI());
}

// Tests that, when the FRE dialog is already open in an inactive tab and some
// other modal is open in the active tab, trying to show the FRE does nothing.
IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       InactiveTabFreDialogNotClosedWhenBlockedByModalUI) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreShow();
  tabs::TabInterface* original_tab =
      GetTabInterfaceForActiveWebContents(browser());

  // Open a new tab at the end of the tab strip and activate it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(1);
  tabs::TabInterface* new_tab = GetTabInterfaceForActiveWebContents(browser());

  // Show some other modal in the active tab.
  auto scoped_tab_modal_ui = new_tab->ShowModalUI();
  // The FRE should be blocked from showing, the existing FRE should not close,
  // and a new FRE should not be opened in the active tab.
  EXPECT_FALSE(glic_fre_controller().CanShowFreDialog(browser()));
  glic_fre_controller().DismissFreIfOpenOnActiveTab(browser());
  WaitForFreShow();
  EXPECT_FALSE(original_tab->CanShowModalUI());
}

// Test proper destruction of the FRE controller when the WebContents is
// destroyed.
IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreControllerWithWebContentsDestruction) {
  // Open the FRE dialog in a tab.
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreShow();

  // Open a new tab at the end of the tab strip and activate it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(1);

  // Destroy the WebContents that the dialog is being shown on.
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);

  WaitForFreClose();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Fre.WidgetClosedReason2",
      /*sample=*/glic::GlicFreWidgetClosedReason::kHostTabClosed,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest, FreAcceptance) {
  if (base::FeatureList::IsEnabled(features::kGlicMultiInstance)) {
    // TODO(b/453696965): Broken in multi-instance.
    GTEST_SKIP() << "Skipping for kGlicMultiInstance";
  }
  // Open the FRE dialog in a tab.
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreShow();

  // Accept the FRE and confirm it closed and the glic panel opened.
  glic_fre_controller().AcceptFre(/*handler=*/nullptr);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.Accept"), 1);
  WaitForFreClose();
  WaitForGlicPanelShow();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Fre.WidgetClosedReason2",
      /*sample=*/glic::GlicFreWidgetClosedReason::kAcceptButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest, DoNotCrashOnBrowserClose) {
  // Open the FRE dialog in a tab.
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreShow();

  chrome::CloseAllBrowsers();
  histogram_tester_.ExpectUniqueSample(
      "Glic.Fre.WidgetClosedReason2",
      /*sample=*/glic::GlicFreWidgetClosedReason::kHostTabClosed,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       NoPanelClosedActionOnBrowserCloseBeforeShow) {
  // Close the browser, which should not log a "panel closed" user action.
  chrome::CloseAllBrowsers();

  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.ErrorPanelClosed"), 0);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Glic.Fre.DisabledByAdminPanelClosed"),
      0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.OfflinePanelClosed"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.LoadingPanelClosed"),
            0);
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.ReadyPanelClosed"), 0);
  EXPECT_EQ(
      user_action_tester_.GetActionCount("Glic.Fre.UninitializedPanelClosed"),
      0);
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       PanelClosedActionOnBrowserCloseAfterShow) {
  // Open the FRE dialog in a tab.
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreInitialized();

  // Close the browser, which should not log a "panel closed" user action.
  chrome::CloseAllBrowsers();

  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.LoadingPanelClosed"),
            1);
  histogram_tester_.ExpectUniqueSample(
      "Glic.Fre.WidgetClosedReason2",
      /*sample=*/glic::GlicFreWidgetClosedReason::kHostTabClosed,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest, FreRejection) {
  // Open the FRE dialog in a tab.
  glic_fre_controller().ShowFreDialog(
      browser(), mojom::InvocationSource::kTopChromeButton);
  WaitForFreShow();

  // Reject the FRE and confirm it closed.
  glic_fre_controller().RejectFre();
  EXPECT_EQ(user_action_tester_.GetActionCount("Glic.Fre.NoThanks"), 1);
  WaitForFreClose();

  // Verify the close reason was logged correctly.
  histogram_tester_.ExpectUniqueSample(
      "Glic.Fre.WidgetClosedReason2",
      /*sample=*/glic::GlicFreWidgetClosedReason::kCancelButtonClicked,
      /*expected_bucket_count=*/1);
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreAcceptanceClosesOtherHandlers) {
  // Open two tabs so there are valid WebContents for the handlers.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  content::WebContents* wc1 = browser()->tab_strip_model()->GetWebContentsAt(0);
  content::WebContents* wc2 = browser()->tab_strip_model()->GetWebContentsAt(1);
  // Show FRE (initializes controller state) in 2 tabs and create 2 page
  // handlers.
  glic_service()->ToggleUI(browser(), true,
                           mojom::InvocationSource::kTopChromeButton);
  FakeGlicFrePageHandler handler1(/*is_unified=*/true, wc1,
                                  glic_fre_controller());
  glic_service()->ToggleUI(browser(), true,
                           mojom::InvocationSource::kTopChromeButton);
  FakeGlicFrePageHandler handler2(/*is_unified=*/true, wc2,
                                  glic_fre_controller());

  // Handler 1 accepts.
  handler1.SimulateAccept();

  // Verify that a sample was recorded in each histogram, regardless of its
  // value.
  histogram_tester_.ExpectTotalCount("Glic.UnifiedFre.TotalTime.Accepted", 1);
  histogram_tester_.ExpectTotalCount(
      "Glic.UnifiedFre.TotalTime.AcceptedByOtherInstance", 1);
  // Ensure no generic dismissal was logged.
  histogram_tester_.ExpectTotalCount("Glic.Fre.TotalTime.Dismissed", 0);
  histogram_tester_.ExpectTotalCount("Glic.UnifiedFre.TotalTime.Dismissed", 0);
}

}  // namespace
}  // namespace glic
