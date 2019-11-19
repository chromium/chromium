// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_dialog_controller.h"

#include <memory>
#include <string>

#include "build/build_config.h"
#include "chrome/browser/printing/print_preview_test.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/test/mock_render_process_host.h"
#include "content/public/test/navigation_simulator.h"
#include "content/public/test/web_contents_tester.h"

using content::WebContents;
using content::WebContentsObserver;

namespace {
// content::WebContentsDelegate destructor is protected: subclass for testing.
class TestWebContentsDelegate : public content::WebContentsDelegate {};

class PrintPreviewDialogDestroyedObserver : public WebContentsObserver {
 public:
  explicit PrintPreviewDialogDestroyedObserver(WebContents* dialog)
      : WebContentsObserver(dialog) {}
  ~PrintPreviewDialogDestroyedObserver() override = default;

  bool dialog_destroyed() const { return dialog_destroyed_; }

 private:
  // content::WebContentsObserver implementation.
  void WebContentsDestroyed() override { dialog_destroyed_ = true; }

  bool dialog_destroyed_ = false;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogDestroyedObserver);
};

}  // namespace

namespace printing {

using PrintPreviewDialogControllerUnitTest = PrintPreviewTest;

// Create/Get a preview dialog for initiator.
TEST_F(PrintPreviewDialogControllerUnitTest, GetOrCreatePreviewDialog) {
  // Lets start with one window with one tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator contents.
  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();

  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Get the preview dialog for initiator.
  PrintViewManager::FromWebContents(initiator)->PrintPreviewNow(
      initiator->GetMainFrame(), false);
  WebContents* preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator);

  // New print preview dialog is a constrained window, so the number of tabs is
  // still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(initiator, preview_dialog);

  // Get the print preview dialog for the same initiator.
  WebContents* new_preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator);

  // Preview dialog already exists. Tab count remains the same.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // 1:1 relationship between initiator and preview dialog.
  EXPECT_EQ(new_preview_dialog, preview_dialog);
}

// Tests multiple print preview dialogs exist in the same browser for different
// initiators. If a preview dialog already exists for an initiator, that
// initiator gets focused.
//
// Flaky on Mac. https://crbug.com/845844
#if defined(OS_MACOSX)
#define MAYBE_MultiplePreviewDialogs DISABLED_MultiplePreviewDialogs
#else
#define MAYBE_MultiplePreviewDialogs MultiplePreviewDialogs
#endif
TEST_F(PrintPreviewDialogControllerUnitTest, MAYBE_MultiplePreviewDialogs) {
  // Lets start with one window and two tabs.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  EXPECT_EQ(0, tab_strip_model->count());

  // Create some new initiators.
  chrome::NewTab(browser());
  WebContents* web_contents_1 = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_1);

  chrome::NewTab(browser());
  WebContents* web_contents_2 = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_2);
  EXPECT_EQ(2, tab_strip_model->count());

  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Create preview dialog for |web_contents_1|
  PrintViewManager::FromWebContents(web_contents_1)
      ->PrintPreviewNow(web_contents_1->GetMainFrame(), false);
  WebContents* preview_dialog_1 =
      dialog_controller->GetOrCreatePreviewDialog(web_contents_1);

  EXPECT_NE(web_contents_1, preview_dialog_1);
  EXPECT_EQ(2, tab_strip_model->count());

  // Create preview dialog for |web_contents_2|
  PrintViewManager::FromWebContents(web_contents_2)
      ->PrintPreviewNow(web_contents_2->GetMainFrame(), false);
  WebContents* preview_dialog_2 =
      dialog_controller->GetOrCreatePreviewDialog(web_contents_2);

  EXPECT_NE(web_contents_2, preview_dialog_2);
  EXPECT_NE(preview_dialog_1, preview_dialog_2);
  // 2 initiators and 2 preview dialogs exist in the same browser.  The preview
  // dialogs are constrained in their respective initiators.
  EXPECT_EQ(2, tab_strip_model->count());

  int tab_1_index = tab_strip_model->GetIndexOfWebContents(web_contents_1);
  int tab_2_index = tab_strip_model->GetIndexOfWebContents(web_contents_2);
  int preview_dialog_1_index =
      tab_strip_model->GetIndexOfWebContents(preview_dialog_1);
  int preview_dialog_2_index =
      tab_strip_model->GetIndexOfWebContents(preview_dialog_2);

  // Constrained dialogs are not in the TabStripModel.
  EXPECT_EQ(-1, preview_dialog_1_index);
  EXPECT_EQ(-1, preview_dialog_2_index);

  // Since |preview_dialog_2_index| was the most recently created dialog, its
  // initiator should have focus.
  EXPECT_EQ(tab_2_index, tab_strip_model->active_index());

  // When we get the preview dialog for |web_contents_1|,
  // |preview_dialog_1| is activated and focused.
  dialog_controller->GetOrCreatePreviewDialog(web_contents_1);
  EXPECT_EQ(tab_1_index, tab_strip_model->active_index());
}

// Check clearing the initiator details associated with a print preview dialog
// allows the initiator to create another print preview dialog.
TEST_F(PrintPreviewDialogControllerUnitTest, ClearInitiatorDetails) {
  // Lets start with one window with one tab.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator contents.
  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();

  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Get the preview dialog for the initiator.
  PrintViewManager::FromWebContents(initiator)->PrintPreviewNow(
      initiator->GetMainFrame(), false);
  WebContents* preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator);

  // New print preview dialog is a constrained window, so the number of tabs is
  // still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(initiator, preview_dialog);

  // Clear the initiator details associated with the preview dialog.
  dialog_controller->EraseInitiatorInfo(preview_dialog);

  // Get a new print preview dialog for the initiator.
  WebContents* new_preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(initiator);

  // New print preview dialog is a constrained window, so the number of tabs is
  // still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  // Verify a new print preview dialog has been created.
  EXPECT_NE(new_preview_dialog, preview_dialog);
}

// Test that print preview dialogs close on navigation to new pages
// and when navigating to old pages via fwd/back, but that auto navigation
// (typed + address bar) to an existing page as occurs in gmail does not cause
// the dialogs to close.
TEST_F(PrintPreviewDialogControllerUnitTest, CloseDialogOnNavigation) {
  // Two similar URLs (same webpage, different URL fragments)
  GURL tiger_barb("https://www.google.com/#q=tiger+barb");
  GURL tiger("https://www.google.com/#q=tiger");

  // Set up by opening a new tab and getting web contents
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(0, browser()->tab_strip_model()->count());
  chrome::NewTab(browser());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Navigate to first page
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents,
                                                             tiger);
  EXPECT_EQ(tiger, web_contents->GetLastCommittedURL());

  // Get the preview dialog
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);
  WebContents* tiger_preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(web_contents);
  PrintViewManager* manager = PrintViewManager::FromWebContents(web_contents);
  manager->PrintPreviewNow(web_contents->GetMainFrame(), false);

  // New print preview dialog is a constrained window, so the number of tabs is
  // still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(web_contents, tiger_preview_dialog);
  PrintPreviewDialogDestroyedObserver tiger_destroyed(tiger_preview_dialog);

  // Navigate via link to a similar page.
  content::NavigationSimulator::NavigateAndCommitFromBrowser(web_contents,
                                                             tiger_barb);

  // Check navigation was successful
  EXPECT_EQ(tiger_barb, web_contents->GetLastCommittedURL());

  // Print preview now should return true as the navigation should have closed
  // |tiger_preview_dialog| and the previous dialog should have closed.
  EXPECT_TRUE(manager->PrintPreviewNow(web_contents->GetMainFrame(), false));
  WebContents* tiger_barb_preview_dialog =
      dialog_controller->GetOrCreatePreviewDialog(web_contents);
  ASSERT_TRUE(tiger_barb_preview_dialog);

  // Check a new dialog was created - either the pointers should be different or
  // the previous web contents must have been destroyed.
  EXPECT_TRUE(tiger_destroyed.dialog_destroyed() ||
              tiger_barb_preview_dialog != tiger_preview_dialog);
  EXPECT_NE(tiger_barb_preview_dialog, web_contents);
  PrintPreviewDialogDestroyedObserver tiger_barb_destroyed(
      tiger_barb_preview_dialog);

  // Now this returns false as |tiger_barb_preview_dialog| is open.
  EXPECT_FALSE(manager->PrintPreviewNow(web_contents->GetMainFrame(), false));

  // Navigate with back button or ALT+LEFT ARROW to a similar page.
  content::NavigationSimulator::GoBack(web_contents);
  EXPECT_EQ(tiger, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(manager->PrintPreviewNow(web_contents->GetMainFrame(), false));

  // Get new dialog
  WebContents* tiger_preview_dialog_2 =
      dialog_controller->GetOrCreatePreviewDialog(web_contents);
  ASSERT_TRUE(tiger_preview_dialog_2);

  // Verify this is a new dialog.
  EXPECT_TRUE(tiger_barb_destroyed.dialog_destroyed() ||
              tiger_barb_preview_dialog != tiger_preview_dialog_2);
  EXPECT_NE(tiger_preview_dialog_2, web_contents);
  PrintPreviewDialogDestroyedObserver tiger_2_destroyed(
      tiger_preview_dialog_2);

  // Try to simulate Gmail navigation: Navigate to an existing page (via
  // Forward) but modify the navigation type while pending to look like an
  // address bar + typed transition (like Gmail auto navigation)
  content::NavigationController& nav_controller = web_contents->GetController();
  nav_controller.GoForward();
  nav_controller.GetPendingEntry()->SetTransitionType(ui::PageTransitionFromInt(
      ui::PAGE_TRANSITION_TYPED | ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  CommitPendingLoad(&nav_controller);

  // Navigation successful
  EXPECT_EQ(tiger_barb, web_contents->GetLastCommittedURL());

  // Print preview should not have changed due to this navigation type so print
  // preview now should return false, dialog is still alive, and the dialog
  // returned by GetOrCreatePreviewDialog should be the same as the earlier
  // dialog.
  EXPECT_FALSE(manager->PrintPreviewNow(web_contents->GetMainFrame(), false));
  EXPECT_FALSE(tiger_2_destroyed.dialog_destroyed());
  WebContents* tiger_preview_dialog_2b =
      dialog_controller->GetOrCreatePreviewDialog(web_contents);
  ASSERT_TRUE(tiger_preview_dialog_2b);
  EXPECT_EQ(tiger_preview_dialog_2b, tiger_preview_dialog_2);
  EXPECT_NE(tiger_preview_dialog_2b, web_contents);
}

// Tests preview dialog controller cleans up correctly and does not throw errors
// on a renderer process crash. Checks that the renderer process closed
// notification is still received even if one of two preview dialogs with the
// same renderer process host is closed before the process "crashes".
TEST_F(PrintPreviewDialogControllerUnitTest, MultiplePreviewDialogsClose) {
  // Set up the browser.
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  EXPECT_EQ(0, tab_strip_model->count());

  // Create a new tab with contents |web_contents_1|
  chrome::NewTab(browser());
  WebContents* web_contents_1 = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_1);
  EXPECT_EQ(1, tab_strip_model->count());
  PrintPreviewDialogController* dialog_controller =
      PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Create preview dialog for |web_contents_1|. Should not create a new tab.
  PrintViewManager::FromWebContents(web_contents_1)
      ->PrintPreviewNow(web_contents_1->GetMainFrame(), false);
  WebContents* preview_dialog_1 =
      dialog_controller->GetOrCreatePreviewDialog(web_contents_1);
  EXPECT_NE(web_contents_1, preview_dialog_1);
  EXPECT_EQ(1, tab_strip_model->count());

  // Create a new tab with contents |web_contents_2|
  chrome::NewTab(browser());
  WebContents* web_contents_2 = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_2);
  EXPECT_EQ(2, tab_strip_model->count());

  // Create preview dialog for |web_contents_2|
  PrintViewManager::FromWebContents(web_contents_2)
      ->PrintPreviewNow(web_contents_2->GetMainFrame(), false);
  WebContents* preview_dialog_2 =
      dialog_controller->GetOrCreatePreviewDialog(web_contents_2);
  EXPECT_NE(web_contents_2, preview_dialog_2);
  EXPECT_NE(preview_dialog_1, preview_dialog_2);

  // 2 initiators and 2 preview dialogs exist in the same browser.  The preview
  // dialogs are constrained in their respective initiators.
  EXPECT_EQ(2, tab_strip_model->count());

  // Close |web_contents_1|'s tab
  int tab_1_index = tab_strip_model->GetIndexOfWebContents(web_contents_1);
  tab_strip_model->CloseWebContentsAt(tab_1_index, 0);
  EXPECT_EQ(1, tab_strip_model->count());

  // Simulate a crash of the render process host for |web_contents_2|. Print
  // preview controller should exit cleanly and not crash.
  content::MockRenderProcessHost* rph =
      static_cast<content::MockRenderProcessHost*>(
          web_contents_2->GetMainFrame()->GetProcess());
  rph->SimulateCrash();
}

}  // namespace printing
