// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/fre/glic_fre_controller.h"

#include "base/test/run_until.h"
#include "base/time/time.h"
#include "chrome/browser/glic/glic_keyed_service.h"
#include "chrome/browser/glic/glic_pref_names.h"
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

namespace glic {
namespace {

class GlicFreControllerBrowserTest : public NonInteractiveGlicTest {
 public:
  GlicFreControllerBrowserTest() = default;
  ~GlicFreControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    glic::test::InteractiveGlicTest::SetUpOnMainThread();
    glic_test_environment().SetFRECompletion(prefs::FreStatus::kNotStarted);
  }

  GlicFreController* glic_fre_controller() {
    return glic_test_environment()
        .GetService()
        ->window_controller()
        .fre_controller();
  }

  tabs::TabInterface* GetTabInterfaceForActiveWebContents(Browser* browser) {
    content::WebContents* tab_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    return tabs::TabInterface::MaybeGetFromContents(tab_web_contents);
  }

  void WaitForFreShow() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return glic_fre_controller()->IsShowingDialog();
    })) << "FRE dialog should have been shown";
  }

  void WaitForFreClose() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return !glic_fre_controller()->IsShowingDialog();
    })) << "FRE dialog should have been closed";
  }

  void WaitForGlicPanelShow() {
    ASSERT_TRUE(base::test::RunUntil([&]() {
      return glic_test_environment().GetService()->IsWindowShowing();
    })) << "Glic panel should have been shown";
  }

  void EnsureFreDoesNotShow() {
    auto end_time = base::TimeTicks::Now() + base::Milliseconds(500);
    ASSERT_TRUE(base::test::RunUntil(
        [&]() { return end_time < base::TimeTicks::Now(); }));
    ASSERT_FALSE(glic_fre_controller()->IsShowingDialog())
        << "FRE dialog should not have been shown";
  }
};

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreDialogShowBlockedByModalUI) {
  tabs::TabInterface* tab = GetTabInterfaceForActiveWebContents(browser());

  // FRE dialog should be blocked from showing if another modal dialog is
  // already open.
  auto scoped_tab_modal_ui = tab->ShowModalUI();
  EXPECT_FALSE(glic_fre_controller()->CanShowFreDialog(browser()));

  // The FRE dialog should be able to open after the existing modal dialog
  // is closed.
  scoped_tab_modal_ui.reset();
  EXPECT_TRUE(glic_fre_controller()->CanShowFreDialog(browser()));
  glic_fre_controller()->ShowFreDialog(browser());

  // Verify the FRE dialog is shown.
  WaitForFreShow();
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreDialogBlocksOtherModalUI) {
  tabs::TabInterface* tab = GetTabInterfaceForActiveWebContents(browser());

  // The FRE dialog should be able to open with no other modal dialogs open.
  EXPECT_TRUE(glic_fre_controller()->CanShowFreDialog(browser()));
  glic_fre_controller()->ShowFreDialog(browser());

  // Verify the FRE dialog is shown.
  WaitForFreShow();

  // Verify that another modal dialog cannot be shown now that the FRE is open.
  EXPECT_FALSE(tab->CanShowModalUI());

  // Once the FRE is closed, other modal dialogs can be shown again.
  glic_fre_controller()->DismissFre();
  EXPECT_TRUE(tab->CanShowModalUI());
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreDialogShowBlockedByItself) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  EXPECT_TRUE(glic_fre_controller()->CanShowFreDialog(browser()));
  glic_fre_controller()->ShowFreDialog(browser());
  WaitForFreShow();

  // Showing the FRE should be blocked as it is already open in the same tab.
  EXPECT_FALSE(glic_fre_controller()->CanShowFreDialog(browser()));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       DismissFreDialogOnActiveTab) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  glic_fre_controller()->ShowFreDialog(browser());
  WaitForFreShow();

  // Close the FRE on the active tab.
  glic_fre_controller()->DismissFreIfOpenOnActiveTab(browser());
  EXPECT_FALSE(glic_fre_controller()->IsShowingDialog());
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       ShowFreDialogOnFailedCookieSync) {
  glic_test_environment().SetResultForFutureCookieSyncInFre(false);
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  glic_fre_controller()->ShowFreDialog(browser());
  WaitForFreShow();
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       DontDismissFreDialogOnInactiveTab) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  glic_fre_controller()->ShowFreDialog(browser());
  WaitForFreShow();

  // Open a new tab at the end of the tab strip and activate it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(1);

  // Attempting to close the FRE on the active tab should do nothing.
  glic_fre_controller()->DismissFreIfOpenOnActiveTab(browser());
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialog());
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
  glic_fre_controller()->ShowFreDialog(browser());
  WaitForFreShow();
  tabs::TabInterface* original_tab =
      GetTabInterfaceForActiveWebContents(browser());

  // Open a new tab at the end of the tab strip and activate it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(1);
  tabs::TabInterface* new_tab = GetTabInterfaceForActiveWebContents(browser());

  // Opening the FRE dialog should close the existing dialog.
  EXPECT_TRUE(glic_fre_controller()->CanShowFreDialog(browser()));
  glic_fre_controller()->ShowFreDialog(browser());
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
  glic_fre_controller()->ShowFreDialog(browser());
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
  EXPECT_FALSE(glic_fre_controller()->CanShowFreDialog(browser()));
  glic_fre_controller()->DismissFreIfOpenOnActiveTab(browser());
  WaitForFreShow();
  EXPECT_FALSE(original_tab->CanShowModalUI());
}

// Test proper destruction of the FRE controller when the WebContents is
// destroyed.
IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreControllerWithWebContentsDestruction) {
  // Open the FRE dialog in a tab.
  glic_fre_controller()->ShowFreDialog(browser());
  WaitForFreShow();

  // Open a new tab at the end of the tab strip and activate it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(1);

  // Destroy the WebContents that the dialog is being shown on.
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest, FreAcceptance) {
  // Open the FRE dialog in a tab.
  glic_fre_controller()->ShowFreDialog(browser());
  WaitForFreShow();

  // Accept the FRE and confirm it closed and the glic panel opened.
  glic_fre_controller()->AcceptFre();
  WaitForFreClose();
  WaitForGlicPanelShow();
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest, DoNotCrashOnBrowserClose) {
  // Open the FRE dialog in a tab.
  glic_fre_controller()->ShowFreDialog(browser());
  WaitForFreShow();

  chrome::CloseAllBrowsers();
}

}  // namespace
}  // namespace glic
