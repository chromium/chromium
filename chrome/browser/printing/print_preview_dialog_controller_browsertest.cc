// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_dialog_controller.h"

#include <memory>

#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "build/build_config.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/printing/test_print_preview_dialog_cloned_observer.h"
#include "chrome/browser/printing/test_print_view_manager_for_request_preview.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/mock_web_contents_task_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_content_client.h"
#include "chrome/common/url_constants.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/common/webplugininfo.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/scoped_accessibility_mode_override.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_message_macros.h"
#include "ui/accessibility/ax_mode.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"

using content::WebContents;
using content::WebContentsObserver;

namespace {

void PluginsLoadedCallback(
    base::OnceClosure quit_closure,
    const std::vector<content::WebPluginInfo>& /* info */) {
  std::move(quit_closure).Run();
}

void CheckPdfPluginForRenderFrame(content::RenderFrameHost* frame) {
  static const base::FilePath kPdfInternalPluginPath(
      ChromeContentClient::kPDFInternalPluginPath);

  content::WebPluginInfo pdf_internal_plugin_info;
  ASSERT_TRUE(content::PluginService::GetInstance()->GetPluginInfoByPath(
      kPdfInternalPluginPath, &pdf_internal_plugin_info));

  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  EXPECT_TRUE(filter->IsPluginAvailable(frame->GetBrowserContext(),
                                        pdf_internal_plugin_info));
}

}  // namespace

class PrintPreviewDialogControllerBrowserTest : public InProcessBrowserTest {
 public:
  WebContents* initiator() {
    return initiator_;
  }

  void PrintPreview() {
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

  void PrintPreviewDone() { test_print_view_manager_->PrintPreviewDone(); }

  void SetAlwaysOpenPdfExternallyForTests() {
    PluginPrefs::GetForProfile(browser()->profile())
        ->SetAlwaysOpenPdfExternallyForTests(true);
  }

 private:
  void SetUpOnMainThread() override {
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
    // Since TestPrintViewManagerForRequestPreview is created with
    // PrintViewManager::UserDataKey(), the real PrintViewManager is not created
    // and TestPrintViewManagerForRequestPreview gets mojo messages for the
    // purposes of this test.
    cloned_tab_observer_ =
        std::make_unique<printing::TestPrintPreviewDialogClonedObserver>(
            first_tab);
    chrome::DuplicateTab(browser());

    initiator_ = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(initiator_);
    ASSERT_NE(first_tab, initiator_);

    test_print_view_manager_ =
        printing::TestPrintViewManagerForRequestPreview::FromWebContents(
            initiator_);
    content::PluginService::GetInstance()->Init();
  }

  void TearDownOnMainThread() override {
    cloned_tab_observer_.reset();
    initiator_ = nullptr;
  }

  std::unique_ptr<printing::TestPrintPreviewDialogClonedObserver>
      cloned_tab_observer_;
  raw_ptr<printing::TestPrintViewManagerForRequestPreview,
          AcrossTasksDanglingUntriaged>
      test_print_view_manager_;
  raw_ptr<WebContents, AcrossTasksDanglingUntriaged> initiator_ = nullptr;
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
                                           GURL(chrome::kChromeUINewTabURL)));
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
  // Make sure plugins are loaded.
  {
    base::RunLoop run_loop;
    content::PluginService::GetInstance()->GetPlugins(
        base::BindOnce(&PluginsLoadedCallback, run_loop.QuitClosure()));
    run_loop.Run();
  }
  // Get the PDF plugin info.
  content::WebPluginInfo pdf_external_plugin_info;
  ASSERT_TRUE(content::PluginService::GetInstance()->GetPluginInfoByPath(
      base::FilePath(ChromeContentClient::kPDFExtensionPluginPath),
      &pdf_external_plugin_info));

  // Disable the PDF plugin.
  SetAlwaysOpenPdfExternallyForTests();

  // Make sure it is actually disabled for webpages.
  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  EXPECT_FALSE(filter->IsPluginAvailable(initiator()->GetBrowserContext(),
                                         pdf_external_plugin_info));

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

namespace {

std::u16string GetExpectedPrefix() {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRINT_PREFIX,
                                    std::u16string());
}

const std::vector<raw_ptr<task_manager::WebContentsTag, VectorExperimental>>&
GetTrackedTags() {
  return task_manager::WebContentsTagsManager::GetInstance()->tracked_tags();
}

}  // namespace

// TODO(crbug.com/40879071): Flaky on macos12 builds.
#if BUILDFLAG(IS_MAC)
#define MAYBE_TaskManagementTest DISABLED_TaskManagementTest
#else
#define MAYBE_TaskManagementTest TaskManagementTest
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MAYBE_TaskManagementTest) {
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
  EXPECT_TRUE(base::StartsWith(pre_existing_title,
                               expected_prefix,
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
  EXPECT_TRUE(base::StartsWith(title,
                               expected_prefix,
                               base::CompareCase::INSENSITIVE_ASCII));
  PrintPreviewDone();
}

IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       PrintPreviewPdfAccessibility) {
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
