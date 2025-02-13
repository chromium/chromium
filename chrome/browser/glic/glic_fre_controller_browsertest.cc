// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/glic_fre_controller.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/glic/auth_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_tabstrip.h"
#include "chrome/browser/ui/tabs/public/tab_interface.h"
#include "chrome/browser/ui/tabs/tab_enums.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace glic {
namespace {

class GlicFreControllerBrowserTest : public InProcessBrowserTest {
 public:
  GlicFreControllerBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kGlic, features::kTabstripComboButton},
        /*disabled_features=*/{});
  }
  GlicFreControllerBrowserTest(const GlicFreControllerBrowserTest&) = delete;
  GlicFreControllerBrowserTest& operator=(const GlicFreControllerBrowserTest&) =
      delete;

  ~GlicFreControllerBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    identity_env_ = std::make_unique<signin::IdentityTestEnvironment>();

    glic_fre_controller_ = std::make_unique<GlicFreController>(
        browser()->profile(), identity_env_->identity_manager());
  }

  void TearDownOnMainThread() override {
    InProcessBrowserTest::TearDownOnMainThread();
    glic_fre_controller_.reset();
  }

  GlicFreController* glic_fre_controller() {
    return glic_fre_controller_.get();
  }

  tabs::TabInterface* GetTabInterfaceForActiveWebContents(Browser* browser) {
    content::WebContents* tab_web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    return tabs::TabInterface::MaybeGetFromContents(tab_web_contents);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<signin::IdentityTestEnvironment> identity_env_;
  std::unique_ptr<GlicFreController> glic_fre_controller_;
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
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreDialogBlocksOtherModalUI) {
  tabs::TabInterface* tab = GetTabInterfaceForActiveWebContents(browser());

  // The FRE dialog should be able to open with no other modal dialogs open.
  EXPECT_TRUE(glic_fre_controller()->CanShowFreDialog(browser()));
  glic_fre_controller()->ShowFreDialog(browser());

  // Verify the FRE dialog is shown.
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());

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
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());

  // Showing the FRE should be blocked as it is already open in the same tab.
  EXPECT_FALSE(glic_fre_controller()->CanShowFreDialog(browser()));
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       DismissFreDialogOnActiveTab) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  glic_fre_controller()->ShowFreDialog(browser());
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());

  // Close the FRE on the active tab.
  glic_fre_controller()->DismissFreIfOpenOnActiveTab(browser());
  EXPECT_FALSE(glic_fre_controller()->IsShowingDialogForTesting());
}

IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       DontDismissFreDialogOnInactiveTab) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  glic_fre_controller()->ShowFreDialog(browser());
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());

  // Open a new tab at the end of the tab strip and activate it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(1);

  // Attempting to close the FRE on the active tab should do nothing.
  glic_fre_controller()->DismissFreIfOpenOnActiveTab(browser());
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());
}

// Tests that, when the FRE dialog is already open in an inactive tab, trying to
// show it in the active tab closes the existing dialog and opens a new one.
IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreDialogCloseAndReopenForDifferentTab) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(0);
  glic_fre_controller()->ShowFreDialog(browser());
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());
  tabs::TabInterface* original_tab =
      GetTabInterfaceForActiveWebContents(browser());

  // Open a new tab at the end of the tab strip and activate it.
  chrome::AddTabAt(browser(), GURL("about:blank"), -1, true);
  browser()->tab_strip_model()->ActivateTabAt(1);
  tabs::TabInterface* new_tab = GetTabInterfaceForActiveWebContents(browser());

  // Opening the FRE dialog should close the existing dialog.
  EXPECT_TRUE(glic_fre_controller()->CanShowFreDialog(browser()));
  glic_fre_controller()->ShowFreDialog(browser());
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());
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
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());
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
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());
  EXPECT_FALSE(original_tab->CanShowModalUI());
}

// Test proper destruction of the FRE controller when the WebContents is
// destroyed.
IN_PROC_BROWSER_TEST_F(GlicFreControllerBrowserTest,
                       FreControllerWithWebContentsDestruction) {
  // Open the FRE dialog in a tab.
  chrome::AddTabAt(browser(), GURL("about:blank"), 0, true);
  browser()->tab_strip_model()->ActivateTabAt(0);

  glic_fre_controller()->ShowFreDialog(browser());
  EXPECT_TRUE(glic_fre_controller()->IsShowingDialogForTesting());

  // Open a new tab at the end of the tab strip and activate it.
  chrome::AddTabAt(browser(), GURL("about:blank"), 1, true);
  browser()->tab_strip_model()->ActivateTabAt(1);

  // Destroy the WebContents that the dialog is being shown on.
  browser()->tab_strip_model()->CloseWebContentsAt(
      0, TabCloseTypes::CLOSE_USER_GESTURE);
}

}  // namespace
}  // namespace glic
