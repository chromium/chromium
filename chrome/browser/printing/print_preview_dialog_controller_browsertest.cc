// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/printing/print_preview_dialog_controller.h"

#include <memory>

#include "base/bind.h"
#include "base/bind_helpers.h"
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
#include "components/printing/common/print_messages.h"
#include "content/public/browser/browser_accessibility_state.h"
#include "content/public/browser/plugin_service.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/test/browser_test_utils.h"
#include "ipc/ipc_message_macros.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/ui_base_switches.h"
#include "url/gurl.h"
#include "url/origin.h"

using content::WebContents;
using content::WebContentsObserver;

namespace {

class RequestPrintPreviewObserver : public WebContentsObserver {
 public:
  explicit RequestPrintPreviewObserver(WebContents* dialog)
      : WebContentsObserver(dialog) {}
  ~RequestPrintPreviewObserver() override = default;

  void set_quit_closure(base::OnceClosure quit_closure) {
    quit_closure_ = std::move(quit_closure);
  }

 private:
  // content::WebContentsObserver implementation.
  bool OnMessageReceived(const IPC::Message& message,
                         content::RenderFrameHost* render_frame_host) override {
    IPC_BEGIN_MESSAGE_MAP(RequestPrintPreviewObserver, message)
      IPC_MESSAGE_HANDLER(PrintHostMsg_RequestPrintPreview,
                          OnRequestPrintPreview)
      IPC_MESSAGE_UNHANDLED(break)
    IPC_END_MESSAGE_MAP()
    return false;  // Report not handled so the real handler receives it.
  }

  void OnRequestPrintPreview(
      const PrintHostMsg_RequestPrintPreview_Params& /* params */) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                  std::move(quit_closure_));
  }

  base::OnceClosure quit_closure_;

  DISALLOW_COPY_AND_ASSIGN(RequestPrintPreviewObserver);
};

class PrintPreviewDialogClonedObserver : public WebContentsObserver {
 public:
  explicit PrintPreviewDialogClonedObserver(WebContents* dialog)
      : WebContentsObserver(dialog) {}
  ~PrintPreviewDialogClonedObserver() override = default;

  RequestPrintPreviewObserver* request_preview_dialog_observer() {
    return request_preview_dialog_observer_.get();
  }

 private:
  // content::WebContentsObserver implementation.
  void DidCloneToNewWebContents(WebContents* old_web_contents,
                                WebContents* new_web_contents) override {
    request_preview_dialog_observer_ =
        std::make_unique<RequestPrintPreviewObserver>(new_web_contents);
  }

  std::unique_ptr<RequestPrintPreviewObserver> request_preview_dialog_observer_;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogClonedObserver);
};

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
  ~PrintPreviewDialogControllerBrowserTest() override = default;

  WebContents* initiator() {
    return initiator_;
  }

  void PrintPreview() {
    base::RunLoop run_loop;
    request_preview_dialog_observer()->set_quit_closure(run_loop.QuitClosure());
    chrome::Print(browser());
    run_loop.Run();
  }

  WebContents* GetPrintPreviewDialog() {
    printing::PrintPreviewDialogController* dialog_controller =
        printing::PrintPreviewDialogController::GetInstance();
    return dialog_controller->GetPrintPreviewForContents(initiator_);
  }

  void SetAlwaysOpenPdfExternallyForTests() {
    PluginPrefs::GetForProfile(browser()->profile())
        ->SetAlwaysOpenPdfExternallyForTests(true);
  }

 private:
  void SetUpOnMainThread() override {
#if defined(OS_MACOSX)
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kDisableModalAnimations);
#endif

    WebContents* first_tab =
        browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(first_tab);

    // Open a new tab so |cloned_tab_observer_| can see it first and attach a
    // RequestPrintPreviewObserver to it before the real
    // PrintPreviewMessageHandler gets created. Thus enabling
    // RequestPrintPreviewObserver to get messages first for the purposes of
    // this test.
    cloned_tab_observer_ =
        std::make_unique<PrintPreviewDialogClonedObserver>(first_tab);
    chrome::DuplicateTab(browser());

    initiator_ = browser()->tab_strip_model()->GetActiveWebContents();
    ASSERT_TRUE(initiator_);
    ASSERT_NE(first_tab, initiator_);

    content::PluginService::GetInstance()->Init();
  }

  void TearDownOnMainThread() override {
    cloned_tab_observer_.reset();
    initiator_ = nullptr;
  }

  RequestPrintPreviewObserver* request_preview_dialog_observer() {
    return cloned_tab_observer_->request_preview_dialog_observer();
  }

  std::unique_ptr<PrintPreviewDialogClonedObserver> cloned_tab_observer_;
  WebContents* initiator_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(PrintPreviewDialogControllerBrowserTest);
};

// Flaky on Linux: crbug.com/1021545
#if defined(OS_LINUX)
#define MAYBE_NavigateFromInitiatorTab DISABLED_NavigateFromInitiatorTab
#else
#define MAYBE_NavigateFromInitiatorTab NavigateFromInitiatorTab
#endif
// Test to verify that when a initiator navigates, we can create a new preview
// dialog for the new tab contents.
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MAYBE_NavigateFromInitiatorTab) {
  // Print for the first time.
  PrintPreview();

  // Get the preview dialog for the initiator tab.
  WebContents* preview_dialog = GetPrintPreviewDialog();

  // Check a new print preview dialog got created.
  ASSERT_TRUE(preview_dialog);
  ASSERT_NE(initiator(), preview_dialog);

  // Navigate in the initiator tab. Make sure navigating destroys the print
  // preview dialog.
  PrintPreviewDialogDestroyedObserver dialog_destroyed_observer(preview_dialog);
  ui_test_utils::NavigateToURL(browser(), GURL(chrome::kChromeUINewTabURL));
  ASSERT_TRUE(dialog_destroyed_observer.dialog_destroyed());

  // Try printing again.
  PrintPreview();

  // Get the print preview dialog for the initiator tab.
  WebContents* new_preview_dialog = GetPrintPreviewDialog();

  // Check a new preview dialog got created.
  EXPECT_TRUE(new_preview_dialog);
}

// Flaky on Linux: crbug.com/1021545
#if defined(OS_LINUX)
#define MAYBE_ReloadInitiatorTab DISABLED_ReloadInitiatorTab
#else
#define MAYBE_ReloadInitiatorTab ReloadInitiatorTab
#endif
// Test to verify that after reloading the initiator, it creates a new print
// preview dialog.
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MAYBE_ReloadInitiatorTab) {
  // Print for the first time.
  PrintPreview();

  WebContents* preview_dialog = GetPrintPreviewDialog();

  // Check a new print preview dialog got created.
  ASSERT_TRUE(preview_dialog);
  ASSERT_NE(initiator(), preview_dialog);

  // Reload the initiator. Make sure reloading destroys the print preview
  // dialog.
  PrintPreviewDialogDestroyedObserver dialog_destroyed_observer(preview_dialog);
  chrome::Reload(browser(), WindowOpenDisposition::CURRENT_TAB);
  content::WaitForLoadStop(
      browser()->tab_strip_model()->GetActiveWebContents());
  // When Widget::Close is called, a task is posted that will destroy the
  // widget. Here the widget is closed when the navigation commits. Load stop
  // may occur right after the commit, before the widget is destroyed.
  // Execute pending tasks to account for this.
  base::RunLoop().RunUntilIdle();
  ASSERT_TRUE(dialog_destroyed_observer.dialog_destroyed());

  // Try printing again.
  PrintPreview();

  // Create a preview dialog for the initiator tab.
  WebContents* new_preview_dialog = GetPrintPreviewDialog();
  EXPECT_TRUE(new_preview_dialog);
}

// Flaky on Linux: crbug.com/1021545
#if defined(OS_LINUX)
#define MAYBE_PdfPluginDisabled DISABLED_PdfPluginDisabled
#else
#define MAYBE_PdfPluginDisabled PdfPluginDisabled
#endif
// Test to verify that after print preview works even when the PDF plugin is
// disabled for webpages.
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
}

namespace {

base::string16 GetExpectedPrefix() {
  return l10n_util::GetStringFUTF16(IDS_TASK_MANAGER_PRINT_PREFIX,
                                    base::string16());
}

const std::vector<task_manager::WebContentsTag*>& GetTrackedTags() {
  return task_manager::WebContentsTagsManager::GetInstance()->tracked_tags();
}

}  // namespace

// Flaky on Linux: crbug.com/1021545
#if defined(OS_LINUX)
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
  const base::string16 pre_existing_title = pre_existing_task->title();
  const base::string16 expected_prefix = GetExpectedPrefix();
  EXPECT_TRUE(base::StartsWith(pre_existing_title,
                               expected_prefix,
                               base::CompareCase::INSENSITIVE_ASCII));

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
  const base::string16 title = task->title();
  EXPECT_TRUE(base::StartsWith(title,
                               expected_prefix,
                               base::CompareCase::INSENSITIVE_ASCII));
}

// Flaky on Linux: crbug.com/1021545
#if defined(OS_LINUX)
#define MAYBE_PrintPreviewPdfAccessibility DISABLED_PrintPreviewPdfAccessibility
#else
#define MAYBE_PrintPreviewPdfAccessibility PrintPreviewPdfAccessibility
#endif
IN_PROC_BROWSER_TEST_F(PrintPreviewDialogControllerBrowserTest,
                       MAYBE_PrintPreviewPdfAccessibility) {
  content::BrowserAccessibilityState::GetInstance()->EnableAccessibility();
  ui_test_utils::NavigateToURL(browser(), GURL("data:text/html,HelloWorld"));
  PrintPreview();
  WebContents* preview_dialog = GetPrintPreviewDialog();
  WaitForAccessibilityTreeToContainNodeWithName(preview_dialog, "HelloWorld");
}
