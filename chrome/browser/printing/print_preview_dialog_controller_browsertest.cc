// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_dialog_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/plugins/chrome_plugin_service_filter.h"
#include "chrome/browser/plugins/plugin_prefs.h"
#include "chrome/browser/printing/print_view_manager.h"
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
#include "components/printing/common/print.mojom.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::WebContents;
using content::WebContentsObserver;

namespace {

class TestPrintViewManager : public printing::PrintViewManager {
 public:
  explicit TestPrintViewManager(content::WebContents* web_contents)
      : PrintViewManager(web_contents) {}
  TestPrintViewManager(const TestPrintViewManager&) = delete;
  TestPrintViewManager& operator=(const TestPrintViewManager&) = delete;
  ~TestPrintViewManager() override = default;

  static TestPrintViewManager* FromWebContents(WebContents* web_contents) {
    return static_cast<TestPrintViewManager*>(
        printing::PrintViewManager::FromWebContents(web_contents));
  }

  // Create TestPrintViewManager with PrintViewManager::UserDataKey() so that
  // PrintViewManager::FromWebContents() in printing path returns
  // TestPrintViewManager*.
  static void CreateForWebContents(WebContents* web_contents) {
    TestPrintViewManager* print_manager =
        new TestPrintViewManager(web_contents);
    web_contents->SetUserData(printing::PrintViewManager::UserDataKey(),
                              base::WrapUnique(print_manager));
  }

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  // printing::mojom::PrintManagerHost:
  void RequestPrintPreview(
      printing::mojom::RequestPrintPreviewParamsPtr params) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(quit_closure_));
    printing::PrintViewManager::RequestPrintPreview(std::move(params));
  }

  base::OnceClosure quit_closure_;
};

class PrintPreviewDialogClonedObserver : public WebContentsObserver {
 public:
  explicit PrintPreviewDialogClonedObserver(WebContents* dialog)
      : WebContentsObserver(dialog) {}
  PrintPreviewDialogClonedObserver(const PrintPreviewDialogClonedObserver&) =
      delete;
  PrintPreviewDialogClonedObserver& operator=(
      const PrintPreviewDialogClonedObserver&) = delete;
  ~PrintPreviewDialogClonedObserver() override = default;

 private:
  // content::WebContentsObserver implementation.
  void DidCloneToNewWebContents(WebContents* old_web_contents,
                                WebContents* new_web_contents) override {
    TestPrintViewManager::CreateForWebContents(new_web_contents);
  }
};

void PluginsLoadedCallback(
    base::OnceClosure quit_closure,
    const std::vector<content::WebPluginInfo>& /* info */) {
  std::move(quit_closure).Run();
}

bool GetPdfPluginInfo(content::WebPluginInfo* info) {
  static const base::FilePath pdf_plugin_path(
      ChromeContentClient::kPDFPluginPath);
  return content::PluginService::GetInstance()->GetPluginInfoByPath(
      pdf_plugin_path, info);
}

const char kDummyPrintUrl[] = "chrome://print/dummy.pdf";

void CountFrames(int* frame_count,
                 content::RenderFrameHost* frame) {
  ++(*frame_count);
}

void CheckPdfPluginForRenderFrame(content::RenderFrameHost* frame) {
  content::WebPluginInfo pdf_plugin_info;
  ASSERT_TRUE(GetPdfPluginInfo(&pdf_plugin_info));

  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  EXPECT_TRUE(filter->IsPluginAvailable(
      frame->GetProcess()->GetID(), frame->GetRoutingID(), GURL(kDummyPrintUrl),
      url::Origin(), &pdf_plugin_info));
}

}  // namespace

class PrintPreviewDialogControllerBrowserTest : public InProcessBrowserTest {
 public:
  PrintPreviewDialogControllerBrowserTest() = default;
  PrintPreviewDialogControllerBrowserTest(
      const PrintPreviewDialogControllerBrowserTest&) = delete;
  PrintPreviewDialogControllerBrowserTest& operator=(
      const PrintPreviewDialogControllerBrowserTest&) = delete;
  ~PrintPreviewDialogControllerBrowserTest() override = default;

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
#if defined(OS_MAC)
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableModalAnimations);
#endif

    WebContents* first_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(first_tab);

    // Open a new tab so |cloned_tab_observer_| can see it and create a
    // TestPrintViewManager for it before the real PrintViewManager gets
    // created. Since TestPrintViewManager is created with
    // PrintViewManager::UserDataKey(), the real PrintViewManager is not
    // created and TestPrintViewManager gets mojo messages for the
    // purposes of this test.
    cloned_tab_observer_ =
        std::make_unique<PrintPreviewDialogClonedObserver>(first_tab);
    chrome::DuplicateTab(browser());

    initiator_ = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(initiator_);
    ASSERT_NE(first_tab, initiator_);

    test_print_view_manager_ =
        TestPrintViewManager::FromWebContents(initiator_);
    content::PluginService::GetInstance()->Init();
  }

  void TearDownOnMainThread() override {
    cloned_tab_observer_.reset();
    initiator_ = nullptr;
  }


  std::unique_ptr<PrintPreviewDialogClonedObserver> cloned_tab_observer_;
  TestPrintViewManager* test_print_view_manager_;
  WebContents* initiator_ = nullptr;
};

// Test to verify that when a initiator navigates, we can create a new preview
// dialog for the new tab contents.
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       NavigateFromInitiatorTab) {
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
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
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
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       ReloadInitiatorTab) {
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
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       PdfPluginDisabled) {
  // Make sure plugins are loaded.
  {
    base::RunLoop run_loop;
    content::PluginService::GetInstance()->GetPlugins(
        base::BindOnce(&PluginsLoadedCallback, run_loop.QuitClosure()));
    run_loop.Run();
  }
  // Get the PDF plugin info.
  content::WebPluginInfo pdf_plugin_info;
  ASSERT_TRUE(GetPdfPluginInfo(&pdf_plugin_info));

  // Disable the PDF plugin.
  SetAlwaysOpenPdfExternallyForTests();

  // Make sure it is actually disabled for webpages.
  ChromePluginServiceFilter* filter = ChromePluginServiceFilter::GetInstance();
  content::WebPluginInfo dummy_pdf_plugin_info = pdf_plugin_info;
  EXPECT_FALSE(filter->IsPluginAvailable(
      initiator()->GetMainFrame()->GetProcess()->GetID(),
      initiator()->GetMainFrame()->GetRoutingID(), GURL(),
      url::Origin::Create(GURL("http://google.com")), &dummy_pdf_plugin_info));

  PrintPreview();

  // Check a new print preview dialog got created.
  WebContents* preview_dialog = GetPrintPreviewDialog();
  ASSERT_TRUE(preview_dialog);
  ASSERT_NE(initiator(), preview_dialog);

  // Wait until the <iframe> in the print preview renderer has loaded.
  // |frame_count| should be 2. The other frame is the main frame.
  const int kExpectedFrameCount = 2;
  int frame_count;
  do {
    base::RunLoop run_loop;
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE, run_loop.QuitClosure(), base::TimeDelta::FromSeconds(1));
    run_loop.Run();

    frame_count = 0;
    preview_dialog->ForEachFrame(
        base::BindRepeating(&CountFrames, base::Unretained(&frame_count)));
  } while (frame_count < kExpectedFrameCount);
  ASSERT_EQ(kExpectedFrameCount, frame_count);

  // Make sure all the frames in the dialog has access to the PDF plugin.
  preview_dialog->ForEachFrame(
      base::BindRepeating(&CheckPdfPluginForRenderFrame));

  PrintPreviewDone();
}

namespace {

std::u16string GetExpectedPrefix() {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRINT_PREFIX,
                                    std::u16string());
}

const std::vector<task_manager::WebContentsTag*>& GetTrackedTags() {
  return task_manager::WebContentsTagsManager::GetInstance()->tracked_tags();
}

}  // namespace

IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       TaskManagementTest) {
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
  ui_test_utils::NavigateToURL(browser(), GURL("about:blank"));
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
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,HelloWorld"));
  PrintPreview();
  WebContents* preview_dialog = GetPrintPreviewDialog();
  WaitForAccessibilityTreeToContainNodeWithName(preview_dialog, "HelloWorld");

  PrintPreviewDone();
}
