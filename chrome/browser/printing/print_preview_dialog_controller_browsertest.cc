// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_dialog_controller.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/preloading/scoped_prewarm_feature_list.h"
#include "chrome/browser/printing/print_preview_browsertest.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/test_print_preview_dialog_cloned_observer.h"
#include "chrome/browser/printing/test_print_view_manager_for_request_preview.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/browser_manager_service.h"
#include "chrome/browser/ui/browser_manager_service_factory.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/webui/print_preview/print_preview_ui.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/dialog_test_browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/url_constants.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "ui/gfx/geometry/size.h"
#include "ui/web_dialogs/web_dialog_delegate.h"
#include "url/gurl.h"

using content::WebContents;
using content::WebContentsObserver;

namespace {

void CheckPdfPluginForRenderFrame(content::RenderFrameHost* frame) {
  std::optional<content::WebPluginInfo> pdf_internal_plugin_info =
      content::PluginService::GetInstance()->GetPluginInfoByPathForTesting(
          base::FilePath(ChromeContentClient::kPDFInternalPluginPath));
  ASSERT_TRUE(pdf_internal_plugin_info.has_value());

  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  EXPECT_TRUE(filter->IsPluginAvailable(frame->GetBrowserContext(),
                                        pdf_internal_plugin_info.value()));
}

std::u16string GetExpectedPrefix() {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRINT_PREFIX,
                                    std::u16string());
}

const std::vector<raw_ptr<task_manager::WebContentsTag, VectorExperimental>>&
GetTrackedTags() {
  return task_manager::WebContentsTagsManager::GetInstance()->tracked_tags();
}

// content::WebContentsDelegate destructor is protected: subclass for testing.
class TestWebContentsDelegate : public content::WebContentsDelegate {};

}  // namespace

class PrintPreviewDialogControllerBrowserTest : public printing::PrintPreviewBrowserTest {
 public:
  PrintPreviewDialogControllerBrowserTest() = default;
  ~PrintPreviewDialogControllerBrowserTest() override = default;

  WebContents* initiator() { return initiator_; }

  void SetUpPrintingScenario() {
#if BUILDFLAG(IS_MAC)
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableModalAnimations);
#endif

    WebContents* first_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(first_tab);

    // Open a new tab so `cloned_tab_observer_` can see it and create a
    // TestPrintViewManagerForRequestPreview for it before the real
    // PrintViewManager gets created.
    cloned_tab_observer_ =
        std::make_unique<printing::TestPrintPreviewDialogClonedObserver>(first_tab);
    chrome::DuplicateTab(browser());

    initiator_ = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(initiator_);
    ASSERT_NE(first_tab, initiator_);

    test_print_view_manager_ =
        printing::TestPrintViewManagerForRequestPreview::FromWebContents(initiator_);
    content::PluginService::GetInstance()->Init();
  }

  void PrintPreview() {
    ASSERT_TRUE(test_print_view_manager_);
    base::RunLoop run_loop;
    test_print_view_manager_->set_quit_closure(run_loop.QuitClosure());
    chrome::Print(browser());
    run_loop.Run();
  }

  WebContents* GetPrintPreviewDialog() {
    printing::PrintPreviewDialogController* dialog_controller =
        printing::PrintPreviewDialogController::GetInstance();
    return dialog_controller->GetPrintPreviewForContents(initiator_);
  }

  void PrintPreviewDone() {
    ASSERT_TRUE(test_print_view_manager_);
    test_print_view_manager_->PrintPreviewDone();
  }

  void SetAlwaysOpenPdfExternallyForTests() {
    PluginPrefs::GetForProfile(browser()->profile())
        ->SetAlwaysOpenPdfExternallyForTests(true);
  }

  void TearDownOnMainThread() override {
    cloned_tab_observer_.reset();
    initiator_ = nullptr;

    std::vector<WebContents*> dialogs_to_destroy;
    printing::PrintPreviewDialogController::GetInstance()->ForEachPreviewDialog(
        base::BindRepeating(
            [](std::vector<WebContents*>* vec, WebContents* dialog) {
              vec->push_back(dialog);
            },
            base::Unretained(&dialogs_to_destroy)));

    for (WebContents* dialog : dialogs_to_destroy) {
      printing::PrintPreviewDialogController::GetInstance()->EraseInitiatorInfo(dialog);
    }

    test_web_contents_.clear();



    for (Browser* browser : browsers_) {
      browser->tab_strip_model()->CloseAllTabs();
    }

    std::vector<Browser*> local_browsers;
    for (Browser* browser : browsers_) {
      local_browsers.push_back(browser);
    }
    browsers_.clear();

    for (Browser* browser : local_browsers) {
      BrowserManagerServiceFactory::GetForProfile(browser->profile())
          ->DeleteBrowser(browser);
    }

    printing::PrintPreviewBrowserTest::TearDownOnMainThread();
  }

  WebContents* CreateTestTab() {
    auto tab = WebContents::Create(
        WebContents::CreateParams(browser()->profile()));
    printing::PrintViewManager::CreateForWebContents(tab.get());
    WebContents* tab_ptr = tab.get();
    test_web_contents_.push_back(std::move(tab));
    return tab_ptr;
  }

 protected:
  Browser* CreateBrowser(std::unique_ptr<BrowserWindow> window) {
    Browser::CreateParams params(browser()->profile(), true);
    params.window = window.release();
    Browser* browser = Browser::Create(params);
    browsers_.push_back(browser);
    return browser;
  }

  void ReleaseTrackedWebContents(WebContents* web_contents) {
    for (auto it = test_web_contents_.begin(); it != test_web_contents_.end();
         ++it) {
      if (it->get() == web_contents) {
        it->release();
        test_web_contents_.erase(it);
        break;
      }
    }
  }

 private:
  // TODO(https://crbug.com/423465927): Explore a better approach to make the
  // existing tests run with the prewarm feature enabled.
  test::ScopedPrewarmFeatureList prewarm_feature_list_{
      test::ScopedPrewarmFeatureList::PrewarmState::kDisabled};

  std::unique_ptr<printing::TestPrintPreviewDialogClonedObserver> cloned_tab_observer_;
  raw_ptr<printing::TestPrintViewManagerForRequestPreview, AcrossTasksDanglingUntriaged>
      test_print_view_manager_;
  raw_ptr<WebContents, AcrossTasksDanglingUntriaged> initiator_ = nullptr;

  std::vector<std::unique_ptr<WebContents>> test_web_contents_;
  std::vector<raw_ptr<Browser>> browsers_;
};

// Test to verify that when a initiator navigates, we can create a new preview
// dialog for the new tab contents.
// TODO(crbug.com/40251696): Test is flaky on Mac
#if BUILDFLAG(IS_MAC)
#define MAYBE_NavigateFromInitiatorTab DISABLED_NavigateFromInitiatorTab
#else
#define MAYBE_NavigateFromInitiatorTab NavigateFromInitiatorTab
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MAYBE_NavigateFromInitiatorTab) {
  SetUpPrintingScenario();

  // Print for the first time.
  PrintPreview();

  // Get the preview dialog for the initiator tab.
  WebContents* preview_dialog = GetPrintPreviewDialog();

  // Check a new print preview dialog got created.
  ASSERT_TRUE(preview_dialog);
  ASSERT_NE(initiator(), preview_dialog);

  PrintPreviewDone();

  // Navigate in the initiator tab. Make sure navigating destroys the print
  // preview dialog.
  content::WebContentsDestroyedWatcher watcher(preview_dialog);
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           chrome::ChromeUINewTabURLAsGURL()));
  ASSERT_TRUE(watcher.IsDestroyed());

  // Try printing again.
  PrintPreview();

  // Get the print preview dialog for the initiator tab.
  WebContents* new_preview_dialog = GetPrintPreviewDialog();

  // Check a new preview dialog got created.
  EXPECT_TRUE(new_preview_dialog);

  PrintPreviewDone();
}

// Test to verify that after reloading the initiator, it creates a new print
// preview dialog.
// TODO(crbug.com/40251696): Test is flaky on Mac
#if BUILDFLAG(IS_MAC)
#define MAYBE_ReloadInitiatorTab DISABLED_ReloadInitiatorTab
#else
#define MAYBE_ReloadInitiatorTab ReloadInitiatorTab
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MAYBE_ReloadInitiatorTab) {
  SetUpPrintingScenario();

  // Print for the first time.
  PrintPreview();

  WebContents* preview_dialog = GetPrintPreviewDialog();

  // Check a new print preview dialog got created.
  ASSERT_TRUE(preview_dialog);
  ASSERT_NE(initiator(), preview_dialog);

  PrintPreviewDone();

  // Reload the initiator. Make sure reloading destroys the print preview
  // dialog.
  content::WebContentsDestroyedWatcher watcher(preview_dialog);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  EXPECT_TRUE(content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents()));
  // When Widget::Close is called, a task is posted that will destroy the
  // widget. Here the widget is closed when the navigation commits. Load stop
  // may occur right after the commit, before the widget is destroyed.
  // Execute pending tasks to account for this.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(watcher.IsDestroyed());

  // Try printing again.
  PrintPreview();

  // Create a preview dialog for the initiator tab.
  WebContents* new_preview_dialog = GetPrintPreviewDialog();
  EXPECT_TRUE(new_preview_dialog);

  PrintPreviewDone();
}

// Test to verify that after print preview works even when the PDF plugin is
// disabled for webpages.
// TODO(crbug.com/40884297): Flaky on Mac12 Test.
#if BUILDFLAG(IS_MAC)
#define MAYBE_PdfPluginDisabled DISABLED_PdfPluginDisabled
#else
#define MAYBE_PdfPluginDisabled PdfPluginDisabled
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MAYBE_PdfPluginDisabled) {
  SetUpPrintingScenario();

  // Make sure plugins are loaded.
  content::PluginService::GetInstance()->GetPlugins();
  // Get the PDF plugin info.
  std::optional<content::WebPluginInfo> pdf_external_plugin_info =
      content::PluginService::GetInstance()->GetPluginInfoByPathForTesting(
          base::FilePath(ChromeContentClient::kPDFExtensionPluginPath));
  ASSERT_TRUE(pdf_external_plugin_info.has_value());

  // Disable the PDF plugin.
  SetAlwaysOpenPdfExternallyForTests();

  // Make sure it is actually disabled for webpages.
  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  EXPECT_FALSE(filter->IsPluginAvailable(initiator()->GetBrowserContext(),
                                         pdf_external_plugin_info.value()));

  PrintPreview();

  // Check a new print preview dialog got created.
  WebContents* preview_dialog = GetPrintPreviewDialog();
  ASSERT_TRUE(preview_dialog);
  ASSERT_NE(initiator(), preview_dialog);

  // Wait until all the frames in the Print Preview renderer have loaded.
  // `frame_count` should be 3: the main frame, the viewer's <iframe>, and the
  // plugin frame.
  const int kExpectedFrameCount = 3;
  int frame_count;
  do {
    base::RunLoop run_loop;
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::Seconds(1));
    run_loop.Run();

    frame_count = 0;
    preview_dialog->GetPrimaryMainFrame()->ForEachRenderFrameHost(
        [&frame_count](content::RenderFrameHost* /*frame*/) { ++frame_count; });
  } while (frame_count < kExpectedFrameCount);
  ASSERT_EQ(kExpectedFrameCount, frame_count);

  // Make sure all the frames in the dialog has access to the PDF plugin.
  preview_dialog->GetPrimaryMainFrame()->ForEachRenderFrameHost(
      &CheckPdfPluginForRenderFrame);

  PrintPreviewDone();
}

// TODO(crbug.com/40879071): Flaky on macos12 builds.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TaskManagementTest DISABLED_TaskManagementTest
#else
#define MAYBE_TaskManagementTest TaskManagementTest
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MAYBE_TaskManagementTest) {
  SetUpPrintingScenario();

  // This test starts with two tabs open.
  EXPECT_EQ(2U, GetTrackedTags().size());

  PrintPreview();
  EXPECT_EQ(3U, GetTrackedTags().size());

  // Create a task manager and expect the pre-existing print previews are
  // provided.
  task_manager::MockWebContentsTaskManager task_manager;
  EXPECT_TRUE(task_manager.tasks().empty());
  task_manager.StartObserving();
  ASSERT_EQ(3U, task_manager.tasks().size());
  const task_manager::Task* pre_existing_task = task_manager.tasks().back();
  EXPECT_EQ(task_manager::Task::RENDERER, pre_existing_task->GetType());
  const std::u16string pre_existing_title = pre_existing_task->title();
  const std::u16string expected_prefix = GetExpectedPrefix();
  EXPECT_TRUE(base::StartsWith(pre_existing_title, expected_prefix,
                               base::CompareCase::INSENSITIVE_ASCII));

  PrintPreviewDone();

  // Navigating away from the current page in the current tab for which a print
  // preview is displayed will cancel the print preview and hence the task
  // manger shouldn't show a printing task.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), GURL("about:blank")));
  EXPECT_EQ(2U, GetTrackedTags().size());
  EXPECT_EQ(2U, task_manager.tasks().size());

  // Now start another print preview after the had already been created and
  // validated that a corresponding task is reported.
  PrintPreview();
  EXPECT_EQ(3U, GetTrackedTags().size());
  ASSERT_EQ(3U, task_manager.tasks().size());
  const task_manager::Task* task = task_manager.tasks().back();
  EXPECT_EQ(task_manager::Task::RENDERER, task->GetType());
  const std::u16string title = task->title();
  EXPECT_TRUE(base::StartsWith(title, expected_prefix,
                               base::CompareCase::INSENSITIVE_ASCII));
  PrintPreviewDone();
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       PrintPreviewPdfAccessibility) {
  SetUpPrintingScenario();

  content::ScopedAccessibilityModeOverride mode_override(ui::kAXModeComplete);
  // Put a DIV after the text we're going to search for. The last node in the
  // tree will not have a newline appended, but all the others will. Avoid
  // making assumptions about whether it's the last node or not. There may be
  // nodes for headers and footers following the document contents.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("data:text/html,HelloWorld<div>next</div>")));
  PrintPreview();
  WebContents* preview_dialog = GetPrintPreviewDialog();
  WaitForAccessibilityTreeToContainNodeWithName(preview_dialog,
                                                "HelloWorld\r\n");

  PrintPreviewDone();
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest, IsPrintPreviewURL) {
  EXPECT_TRUE(printing::PrintPreviewDialogController::IsPrintPreviewURL(
      GURL("chrome://print/fake-path")));
  EXPECT_FALSE(printing::PrintPreviewDialogController::IsPrintPreviewURL(
      GURL("chrome-untrusted://print/fake-path")));
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       IsPrintPreviewContentURL) {
  EXPECT_TRUE(printing::PrintPreviewDialogController::IsPrintPreviewContentURL(
      GURL("chrome-untrusted://print/fake-path")));
  EXPECT_FALSE(printing::PrintPreviewDialogController::IsPrintPreviewContentURL(
      GURL("chrome://print/fake-path")));
}

// Create/Get a preview dialog for initiator.
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       GetOrCreatePreviewDialog) {
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator contents.
  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();

  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  WebContents* preview_dialog_ptr = CreateTestTab();

  // Associate mock dialog cleanly
  dialog_controller->AssociateWebContentsesForTesting(initiator,
                                                      preview_dialog_ptr);

  // New print preview dialog is a constrained window, so the number of tabs is
  // still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(initiator, preview_dialog_ptr);

  // Get the print preview dialog for the same initiator.
  WebContents* new_preview_dialog =
      dialog_controller->GetPrintPreviewForContents(initiator);

  // Preview dialog already exists. Tab count remains the same.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // 1:1 relationship between initiator and preview dialog.
  EXPECT_EQ(new_preview_dialog, preview_dialog_ptr);

  // Cleanup association
  dialog_controller->DisassociateWebContentsesForTesting(preview_dialog_ptr);
}

// Tests multiple print preview dialogs exist in the same browser for different
// initiators. If a preview dialog already exists for an initiator, that
// initiator gets focused.
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MultiplePreviewDialogs) {
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);

  EXPECT_EQ(1, tab_strip_model->count());

  // Create some new initiators.
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  WebContents* web_contents_1 = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_1);

  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  WebContents* web_contents_2 = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_2);
  EXPECT_EQ(3, tab_strip_model->count());

  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  WebContents* preview_dialog_ptr_1 = CreateTestTab();
  WebContents* preview_dialog_ptr_2 = CreateTestTab();

  dialog_controller->AssociateWebContentsesForTesting(web_contents_1,
                                                      preview_dialog_ptr_1);
  dialog_controller->AssociateWebContentsesForTesting(web_contents_2,
                                                      preview_dialog_ptr_2);

  EXPECT_NE(web_contents_1, preview_dialog_ptr_1);
  EXPECT_EQ(3, tab_strip_model->count());

  EXPECT_NE(web_contents_2, preview_dialog_ptr_2);
  EXPECT_NE(preview_dialog_ptr_1, preview_dialog_ptr_2);
  // 3 initiators and 2 preview dialogs exist in the same browser. The preview
  // dialogs are constrained in their respective initiators.
  EXPECT_EQ(3, tab_strip_model->count());

  int tab_1_index = tab_strip_model->GetIndexOfWebContents(web_contents_1);
  int tab_2_index = tab_strip_model->GetIndexOfWebContents(web_contents_2);
  EXPECT_NE(tab_1_index, tab_2_index);

  int preview_dialog_1_index =
      tab_strip_model->GetIndexOfWebContents(preview_dialog_ptr_1);
  int preview_dialog_2_index =
      tab_strip_model->GetIndexOfWebContents(preview_dialog_ptr_2);

  // Constrained dialogs are not in the TabStripModel.
  EXPECT_EQ(-1, preview_dialog_1_index);
  EXPECT_EQ(-1, preview_dialog_2_index);

  // Since `preview_dialog_ptr_2` was the most recently created dialog, its
  // initiator should have focus.
  EXPECT_EQ(tab_2_index, tab_strip_model->active_index());

  // When we get the preview dialog for `web_contents_1`, `preview_dialog_2`
  // remains activated and focused. The previous behavior was to activate
  // `preview_dialog_1`, but that allowed tabs to steal focus.
  EXPECT_EQ(preview_dialog_ptr_1,
            dialog_controller->GetPrintPreviewForContents(web_contents_1));
  EXPECT_EQ(tab_2_index, tab_strip_model->active_index());

  // Cleanup associations
  dialog_controller->DisassociateWebContentsesForTesting(preview_dialog_ptr_1);
  dialog_controller->DisassociateWebContentsesForTesting(preview_dialog_ptr_2);
}

// Check clearing the initiator details associated with a print preview dialog
// allows the initiator to create another print preview dialog.
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       ClearInitiatorDetails) {
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());

  // Create a reference to initiator contents.
  WebContents* initiator = browser()->tab_strip_model()->GetActiveWebContents();

  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  WebContents* preview_dialog_ptr = CreateTestTab();
  dialog_controller->AssociateWebContentsesForTesting(initiator,
                                                      preview_dialog_ptr);

  // New print preview dialog is a constrained window, so the number of tabs is
  // still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(initiator, preview_dialog_ptr);

  // Clear the initiator details associated with the preview dialog.
  dialog_controller->EraseInitiatorInfo(preview_dialog_ptr);

  // Verify the initiator info should be erased
  EXPECT_FALSE(dialog_controller->GetPrintPreviewForContents(initiator));

  // Cleanup association
  dialog_controller->DisassociateWebContentsesForTesting(preview_dialog_ptr);
}

// Test that print preview dialogs close on navigation to new pages
// and when navigating to old pages via fwd/back, but that auto navigation
// (typed + address bar) to an existing page as occurs in gmail does not cause
// the dialogs to close.
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       CloseDialogOnNavigation) {
  // Two similar URLs (same webpage, different URL fragment/query)
  // Gmail navigates from fragment to query when opening an email to print.
  GURL tiger("chrome://version/#q=tiger");
  GURL tiger_barb("chrome://version/#?q=tiger+barb");

  // Set up by using the default tab
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Navigate to first page
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), tiger));
  EXPECT_EQ(tiger, web_contents->GetLastCommittedURL());

  // Get the preview dialog
  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);
  WebContents* tiger_preview_dialog =
      dialog_controller->GetOrCreatePreviewDialogForTesting(web_contents);
  printing::PrintViewManager* manager = printing::PrintViewManager::FromWebContents(web_contents);
  manager->PrintPreviewNow(web_contents->GetPrimaryMainFrame(), false);

  // New print preview dialog is a constrained window, so the number of tabs is
  // still 1.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_NE(web_contents, tiger_preview_dialog);
  content::WebContentsDestroyedWatcher tiger_destroyed(tiger_preview_dialog);

  // Navigate via link to a similar page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), tiger_barb));

  // Check navigation was successful
  EXPECT_EQ(tiger_barb, web_contents->GetLastCommittedURL());

  // Print preview now should return true as the navigation should have closed
  // `tiger_preview_dialog` and the previous dialog should have closed.
  EXPECT_TRUE(
      manager->PrintPreviewNow(web_contents->GetPrimaryMainFrame(), false));
  WebContents* tiger_barb_preview_dialog =
      dialog_controller->GetOrCreatePreviewDialogForTesting(web_contents);
  ASSERT_TRUE(tiger_barb_preview_dialog);

  // Check a new dialog was created - either the pointers should be different or
  // the previous web contents must have been destroyed.
  EXPECT_TRUE(tiger_destroyed.IsDestroyed() ||
              tiger_barb_preview_dialog != tiger_preview_dialog);
  EXPECT_NE(tiger_barb_preview_dialog, web_contents);
  content::WebContentsDestroyedWatcher tiger_barb_destroyed(
      tiger_barb_preview_dialog);

  // Now this returns false as `tiger_barb_preview_dialog` is open.
  EXPECT_FALSE(
      manager->PrintPreviewNow(web_contents->GetPrimaryMainFrame(), false));

  // Navigate with back button or ALT+LEFT ARROW to a similar page.
  web_contents->GetController().GoBack();
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(tiger, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(
      manager->PrintPreviewNow(web_contents->GetPrimaryMainFrame(), false));

  // Get new dialog
  WebContents* tiger_preview_dialog_2 =
      dialog_controller->GetOrCreatePreviewDialogForTesting(web_contents);
  ASSERT_TRUE(tiger_preview_dialog_2);

  // Verify this is a new dialog.
  EXPECT_TRUE(tiger_barb_destroyed.IsDestroyed() ||
              tiger_barb_preview_dialog != tiger_preview_dialog_2);
  EXPECT_NE(tiger_preview_dialog_2, web_contents);
  content::WebContentsDestroyedWatcher tiger_2_destroyed(
      tiger_preview_dialog_2);

  // Try to simulate Gmail navigation: Navigate to an existing page (via
  // Forward) but modify the navigation type while pending to look like an
  // address bar + typed transition (like Gmail auto navigation)
  web_contents->GetController().GoForward();
  ASSERT_TRUE(web_contents->GetController().GetPendingEntry());
  web_contents->GetController().GetPendingEntry()->SetTransitionType(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_TYPED |
                                ui::PAGE_TRANSITION_FROM_ADDRESS_BAR));
  content::WaitForLoadStop(web_contents);
  // Navigation successful
  EXPECT_EQ(tiger_barb, web_contents->GetLastCommittedURL());

  // Print preview should not have changed due to this navigation type so print
  // preview now should return false, dialog is still alive, and the dialog
  // returned by GetOrCreatePreviewDialogForTesting() should be the same as the
  // earlier dialog.
  EXPECT_FALSE(
      manager->PrintPreviewNow(web_contents->GetPrimaryMainFrame(), false));
  EXPECT_FALSE(tiger_2_destroyed.IsDestroyed());
  WebContents* tiger_preview_dialog_2b =
      dialog_controller->GetOrCreatePreviewDialogForTesting(web_contents);
  ASSERT_TRUE(tiger_preview_dialog_2b);
  EXPECT_EQ(tiger_preview_dialog_2b, tiger_preview_dialog_2);
  EXPECT_NE(tiger_preview_dialog_2b, web_contents);

  // Navigate with back button or ALT+LEFT ARROW to a similar page.
  web_contents->GetController().GoBack();
  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(tiger, web_contents->GetLastCommittedURL());
  EXPECT_TRUE(
      manager->PrintPreviewNow(web_contents->GetPrimaryMainFrame(), false));

  // Get new dialog
  WebContents* tiger_preview_dialog_3 =
      dialog_controller->GetOrCreatePreviewDialogForTesting(web_contents);
  ASSERT_TRUE(tiger_preview_dialog_3);

  // Verify this is a new dialog.
  EXPECT_TRUE(tiger_barb_destroyed.IsDestroyed() ||
              tiger_preview_dialog_2 != tiger_preview_dialog_3);
  EXPECT_NE(tiger_preview_dialog_3, web_contents);
  content::WebContentsDestroyedWatcher tiger_3_destroyed(
      tiger_preview_dialog_3);

  // Try to simulate renderer reloading a PWA page: Navigate to an existing page
  // (via Forward) but modify the navigation type while pending to look like a
  // PAGE_TRANSITION_AUTO_BOOKMARK.
  web_contents->GetController().GoForward();
  ASSERT_TRUE(web_contents->GetController().GetPendingEntry());
  web_contents->GetController().GetPendingEntry()->SetTransitionType(
      ui::PageTransitionFromInt(ui::PAGE_TRANSITION_AUTO_BOOKMARK));
  content::WaitForLoadStop(web_contents);
  // Navigation successful
  EXPECT_EQ(tiger_barb, web_contents->GetLastCommittedURL());

  // Print preview should not have changed due to this navigation type so print
  // preview now should return false, dialog is still alive, and the dialog
  // returned by GetOrCreatePreviewDialogForTesting() should be the same as the
  // earlier dialog.
  EXPECT_FALSE(
      manager->PrintPreviewNow(web_contents->GetPrimaryMainFrame(), false));
  EXPECT_FALSE(tiger_3_destroyed.IsDestroyed());
  WebContents* tiger_preview_dialog_3b =
      dialog_controller->GetOrCreatePreviewDialogForTesting(web_contents);
  ASSERT_TRUE(tiger_preview_dialog_3b);
  EXPECT_EQ(tiger_preview_dialog_3b, tiger_preview_dialog_3);
  EXPECT_NE(tiger_preview_dialog_3b, web_contents);
}

// Tests preview dialog controller cleans up correctly and does not throw errors
// on a renderer process crash. Checks that the renderer process closed
// notification is still received even if one of two preview dialogs with the
// same renderer process host is closed before the process "crashes".
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MultiplePreviewDialogsClose) {
  // Set up the browser.
  EXPECT_EQ(1u, GlobalBrowserCollection::GetInstance()->GetSize());
  TabStripModel* tab_strip_model = browser()->tab_strip_model();
  ASSERT_TRUE(tab_strip_model);
  EXPECT_EQ(1, tab_strip_model->count());

  // Create a new tab with contents `web_contents_1`
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  WebContents* web_contents_1 = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_1);
  EXPECT_EQ(2, tab_strip_model->count());
  printing::PrintPreviewDialogController* dialog_controller =
      printing::PrintPreviewDialogController::GetInstance();
  ASSERT_TRUE(dialog_controller);

  // Create preview dialog for `web_contents_1`. Should not create a new tab.
  printing::PrintViewManager::FromWebContents(web_contents_1)
      ->PrintPreviewNow(web_contents_1->GetPrimaryMainFrame(), false);
  WebContents* preview_dialog_1 =
      dialog_controller->GetOrCreatePreviewDialogForTesting(web_contents_1);
  EXPECT_NE(web_contents_1, preview_dialog_1);
  EXPECT_EQ(2, tab_strip_model->count());

  // Create a new tab with contents `web_contents_2`
  chrome::NewTab(browser(), NewTabTypes::kNoUserAction);
  WebContents* web_contents_2 = tab_strip_model->GetActiveWebContents();
  ASSERT_TRUE(web_contents_2);
  EXPECT_EQ(3, tab_strip_model->count());

  // Create preview dialog for `web_contents_2`
  printing::PrintViewManager::FromWebContents(web_contents_2)
      ->PrintPreviewNow(web_contents_2->GetPrimaryMainFrame(), false);
  WebContents* preview_dialog_2 =
      dialog_controller->GetOrCreatePreviewDialogForTesting(web_contents_2);
  EXPECT_NE(web_contents_2, preview_dialog_2);
  EXPECT_NE(preview_dialog_1, preview_dialog_2);

  // 2 initiators and 2 preview dialogs exist in the same browser.  The preview
  // dialogs are constrained in their respective initiators.
  EXPECT_EQ(3, tab_strip_model->count());

  // Close `web_contents_1`'s tab
  int tab_1_index = tab_strip_model->GetIndexOfWebContents(web_contents_1);
  tab_strip_model->CloseWebContentsAt(tab_1_index, 0);
  EXPECT_EQ(2, tab_strip_model->count());

  // Simulate a crash of the render process host for `web_contents_2`. Print
  // preview controller should exit cleanly and not crash.
  content::CrashTab(web_contents_2);
}

class PrintPreviewDialogControllerDialogDelegateTest
    : public PrintPreviewDialogControllerBrowserTest {
 public:
  class DialogTestBrowserWindowWithMaxDialogSize
      : public DialogTestBrowserWindow {
   public:
    explicit DialogTestBrowserWindowWithMaxDialogSize(
        PrintPreviewDialogControllerDialogDelegateTest& owner)
        : owner_(owner) {}
    ~DialogTestBrowserWindowWithMaxDialogSize() override = default;

    // DialogTestBrowserWindow:
    gfx::Size GetMaximumDialogSize() override { return owner_->size(); }
    gfx::NativeView GetHostView() const override { return gfx::NativeView(); }

   private:
    const raw_ref<PrintPreviewDialogControllerDialogDelegateTest> owner_;
  };

  PrintPreviewDialogControllerDialogDelegateTest() = default;
  ~PrintPreviewDialogControllerDialogDelegateTest() override = default;

  std::unique_ptr<ui::WebDialogDelegate> CreateDelegateWithSize(
      const gfx::Size& size) {
    size_ = size;
    auto window =
        std::make_unique<DialogTestBrowserWindowWithMaxDialogSize>(*this);
    Browser* browser = CreateBrowser(std::move(window));

    WebContents* initiator_ptr = CreateTestTab();
    // Add to mock browser's tab strip so standard APIs don't assert
    browser->tab_strip_model()->AppendWebContents(
        std::unique_ptr<WebContents>(initiator_ptr), true);
    // Release ownership from test_web_contents_ as the tab strip now owns it!
    ReleaseTrackedWebContents(initiator_ptr);

    return printing::PrintPreviewDialogController::
        CreatePrintPreviewDialogDelegateForTesting(initiator_ptr);
  }

  const gfx::Size& size() const { return size_; }

 private:
  gfx::Size size_;
};

IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerDialogDelegateTest,
                       GetDialogSizeMinSize) {
  auto delegate = CreateDelegateWithSize({0, 0});
  ASSERT_TRUE(delegate);

  gfx::Size size;
  delegate->GetDialogSize(&size);
  EXPECT_EQ(750, size.width());
  EXPECT_EQ(455, size.height());
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerDialogDelegateTest,
                       GetDialogSizeHD) {
  auto delegate = CreateDelegateWithSize({1920, 1080});
  ASSERT_TRUE(delegate);

  gfx::Size size;
  delegate->GetDialogSize(&size);
  EXPECT_EQ(1309, size.width());
  EXPECT_EQ(863, size.height());
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerDialogDelegateTest,
                       GetDialogSizeUWFHD) {
  auto delegate = CreateDelegateWithSize({2560, 1080});
  ASSERT_TRUE(delegate);

  gfx::Size size;
  delegate->GetDialogSize(&size);
  EXPECT_EQ(1757, size.width());
  EXPECT_EQ(1055, size.height());
}
