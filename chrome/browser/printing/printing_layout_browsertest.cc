// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/path_service.h"
#include "base/process/process_handle.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_file_util.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/printing/print_job.h"
#include "chrome/browser/printing/print_view_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "printing/image.h"
#include "printing/printing_test.h"

namespace {

using printing::Image;

const char kGenerateSwitch[] = "print-layout-generate";

class PrintingLayoutTest : public PrintingTest<InProcessBrowserTest>,
                           public content::NotificationObserver {
 public:
  PrintingLayoutTest() {
    base::FilePath browser_directory;
    base::PathService::Get(chrome::DIR_APP, &browser_directory);
    emf_path_ = browser_directory.AppendASCII("metafile_dumps");
  }

  void SetUp() override {
    // Make sure there is no left overs.
    CleanupDumpDirectory();
    InProcessBrowserTest::SetUp();
  }

  void TearDown() override {
    InProcessBrowserTest::TearDown();
    base::DeleteFile(emf_path_, true);
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitchPath(switches::kDebugPrint, emf_path_);
  }

 protected:
  void PrintNowTab() {
    registrar_.Add(this, chrome::NOTIFICATION_PRINT_JOB_EVENT,
                   content::NotificationService::AllSources());

    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    printing::PrintViewManager::FromWebContents(web_contents)->PrintNow();
    content::RunMessageLoop();
    registrar_.RemoveAll();
  }

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) {
    ASSERT_EQ(chrome::NOTIFICATION_PRINT_JOB_EVENT, type);
    switch (content::Details<printing::JobEventDetails>(details)->type()) {
      case printing::JobEventDetails::JOB_DONE: {
        // Succeeded.
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
        break;
      }
      case printing::JobEventDetails::USER_INIT_CANCELED:
      case printing::JobEventDetails::FAILED: {
        // Failed.
        ASSERT_TRUE(false);
        base::ThreadTaskRunnerHandle::Get()->PostTask(
            FROM_HERE, base::RunLoop::QuitCurrentWhenIdleClosureDeprecated());
        break;
      }
      case printing::JobEventDetails::NEW_DOC:
      case printing::JobEventDetails::USER_INIT_DONE:
      case printing::JobEventDetails::DEFAULT_INIT_DONE:
#if defined(OS_WIN)
      case printing::JobEventDetails::PAGE_DONE:
#endif
      case printing::JobEventDetails::DOC_DONE:
      case printing::JobEventDetails::ALL_PAGES_REQUESTED: {
        // Don't care.
        break;
      }
      default: {
        NOTREACHED();
        break;
      }
    }
  }

  // Finds the dump for the last print job and compares it to the data named
  // |verification_name|. Compares the saved printed job pixels with the test
  // data pixels and returns the percentage of different pixels; 0 for success,
  // [0, 100] for failure.
  double CompareWithResult(const std::wstring& verification_name) {
    base::FilePath test_result(ScanFiles(verification_name));
    if (test_result.value().empty()) {
      // 100% different, the print job buffer is not there.
      return 100.;
    }

    base::FilePath base_path(ui_test_utils::GetTestFilePath(
        base::FilePath().AppendASCII("printing"), base::FilePath()));
    base::FilePath emf(base_path.Append(verification_name + L".emf"));
    base::FilePath png(base_path.Append(verification_name + L".png"));

    base::FilePath cleartype(
        base_path.Append(verification_name + L"_cleartype.png"));
    // Looks for Cleartype override.
    if (base::PathExists(cleartype) && IsClearTypeEnabled())
      png = cleartype;

    if (GenerateFiles()) {
      // Copy the .emf and generate an .png.
      base::CopyFile(test_result, emf);
      Image emf_content(emf);
      emf_content.SaveToPng(png);
      // Saving is always fine.
      return 0;
    } else {
      // File compare between test and result.
      Image emf_content(emf);
      Image test_content(test_result);
      Image png_content(png);
      double diff_emf = emf_content.PercentageDifferent(test_content);

      EXPECT_EQ(0., diff_emf) << base::WideToUTF8(verification_name) <<
          " original size:" << emf_content.size().ToString() <<
          " result size:" << test_content.size().ToString();
      if (diff_emf) {
        // Backup the result emf file.
        base::FilePath failed(
            base_path.Append(verification_name + L"_failed.emf"));
        base::CopyFile(test_result, failed);
      }

      // This verification is only to know that the EMF rendering stays
      // immutable.
      double diff_png = emf_content.PercentageDifferent(png_content);
      EXPECT_EQ(0., diff_png) << base::WideToUTF8(verification_name) <<
          " original size:" << emf_content.size().ToString() <<
          " result size:" << test_content.size().ToString();
      if (diff_png) {
        // Backup the rendered emf file to detect the rendering difference.
        base::FilePath rendering(
            base_path.Append(verification_name + L"_rendering.png"));
        emf_content.SaveToPng(rendering);
      }
      return std::max(diff_png, diff_emf);
    }
  }

  // Makes sure the directory exists and is empty.
  void CleanupDumpDirectory() {
    EXPECT_TRUE(base::DieFileDie(emf_path_, true));
    EXPECT_TRUE(base::CreateDirectory(emf_path_));
  }

  // Returns if Clear Type is currently enabled.
  static bool IsClearTypeEnabled() {
    BOOL ct_enabled = 0;
    if (SystemParametersInfo(SPI_GETCLEARTYPE, 0, &ct_enabled, 0) && ct_enabled)
      return true;
    UINT smoothing = 0;
    if (SystemParametersInfo(SPI_GETFONTSMOOTHINGTYPE, 0, &smoothing, 0) &&
        smoothing == FE_FONTSMOOTHINGCLEARTYPE)
      return true;
    return false;
  }

 private:
  // Verifies that there is one .emf and one .prn file in the dump directory.
  // Returns the path of the .emf file and deletes the .prn file.
  std::wstring ScanFiles(const std::wstring& verification_name) {
    // Try to 10 seconds.
    std::wstring emf_file;
    std::wstring prn_file;
    bool found_emf = false;
    bool found_prn = false;
    for (int i = 0; i < 100; ++i) {
      base::FileEnumerator enumerator(emf_path_, false,
                                      base::FileEnumerator::FILES);
      emf_file.clear();
      prn_file.clear();
      found_emf = false;
      found_prn = false;
      base::FilePath file;
      while (!(file = enumerator.Next()).empty()) {
        std::wstring ext = file.Extension();
        if (base::EqualsCaseInsensitiveASCII(base::WideToUTF8(ext), ".emf")) {
          EXPECT_FALSE(found_emf) << "Found a leftover .EMF file: \"" <<
              emf_file << "\" and \"" << file.value() <<
              "\" when looking for \"" << verification_name << "\"";
          found_emf = true;
          emf_file = file.value();
          continue;
        }
        if (base::EqualsCaseInsensitiveASCII(base::WideToUTF8(ext), ".prn")) {
          EXPECT_FALSE(found_prn) << "Found a leftover .PRN file: \"" <<
              prn_file << "\" and \"" << file.value() <<
              "\" when looking for \"" << verification_name << "\"";
          prn_file = file.value();
          found_prn = true;
          base::DeleteFile(file, false);
          continue;
        }
        EXPECT_TRUE(false);
      }
      if (found_emf && found_prn)
        break;
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(100));
    }
    EXPECT_TRUE(found_emf) << ".PRN file is: " << prn_file;
    EXPECT_TRUE(found_prn) << ".EMF file is: " << emf_file;
    return emf_file;
  }

  static bool GenerateFiles() {
    return base::CommandLine::ForCurrentProcess()->HasSwitch(kGenerateSwitch);
  }

  base::FilePath emf_path_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(PrintingLayoutTest);
};

class PrintingLayoutTextTest : public PrintingLayoutTest {
  typedef PrintingLayoutTest Parent;
 public:
  // Returns if the test is disabled.
  // http://crbug.com/64869 Until the issue is fixed, disable the test if
  // ClearType is enabled.
  static bool IsTestCaseDisabled() {
    return Parent::IsTestCaseDisabled() || IsClearTypeEnabled();
  }
};

// Finds the first dialog window owned by owner_process.
HWND FindDialogWindow(DWORD owner_process) {
  HWND dialog_window(NULL);
  for (;;) {
    dialog_window = FindWindowEx(NULL,
                                 dialog_window,
                                 MAKEINTATOM(32770),
                                 NULL);
    if (!dialog_window)
      break;

    // The dialog must be owned by our target process.
    DWORD process_id = 0;
    GetWindowThreadProcessId(dialog_window, &process_id);
    if (process_id == owner_process)
      break;
  }
  return dialog_window;
}

// Tries to close a dialog window.
bool CloseDialogWindow(HWND dialog_window) {
  LRESULT res = SendMessage(dialog_window, DM_GETDEFID, 0, 0);
  if (!res)
    return false;
  EXPECT_EQ(DC_HASDEFID, HIWORD(res));
  WORD print_button_id = LOWORD(res);
  res = SendMessage(
      dialog_window,
      WM_COMMAND,
      print_button_id,
      reinterpret_cast<LPARAM>(GetDlgItem(dialog_window, print_button_id)));
  return res == 0;
}

// Dismiss the first dialog box owned by owner_process by "executing" the
// default button.
class DismissTheWindow : public base::DelegateSimpleThread::Delegate {
 public:
  DismissTheWindow()
      : owner_process_(base::GetCurrentProcId()) {
  }

  virtual void Run() {
    HWND dialog_window;
    for (;;) {
      // First enumerate the windows.
      dialog_window = FindDialogWindow(owner_process_);

      // Try to close it.
      if (dialog_window) {
        if (CloseDialogWindow(dialog_window)) {
          break;
        }
      }
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
    }

    // Now verify that it indeed closed itself.
    while (IsWindow(dialog_window)) {
      CloseDialogWindow(dialog_window);
      base::PlatformThread::Sleep(base::TimeDelta::FromMilliseconds(10));
    }
  }

  DWORD owner_process() { return owner_process_; }

 private:
  DWORD owner_process_;
};

}  // namespace

// Fails, see http://crbug.com/7721.
IN_PROC_BROWSER_TEST_F(PrintingLayoutTextTest, DISABLED_Complex) {
  if (IsTestCaseDisabled())
    return;

  DismissTheWindow dismisser;
  base::DelegateSimpleThread close_printdlg_thread(&dismisser,
                                                   "close_printdlg_thread");

  // Print a document, check its output.
  ASSERT_TRUE(embedded_test_server()->Start());

  ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL("/printing/test1.html"));
  close_printdlg_thread.Start();
  PrintNowTab();
  close_printdlg_thread.Join();
  EXPECT_EQ(0., CompareWithResult(L"test1"));
}

struct TestPool {
  const char* source;
  const wchar_t* result;
};

const TestPool kTestPool[] = {
    // ImagesB&W
    "/printing/test2.html", L"test2",
    // ImagesTransparent
    "/printing/test3.html", L"test3",
    // ImageColor
    "/printing/test4.html", L"test4",
};

// http://crbug.com/7721
IN_PROC_BROWSER_TEST_F(PrintingLayoutTest, DISABLED_ManyTimes) {
  if (IsTestCaseDisabled())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  DismissTheWindow dismisser;

  ASSERT_GT(base::size(kTestPool), 0u);
  for (int i = 0; i < base::size(kTestPool); ++i) {
    if (i)
      CleanupDumpDirectory();
    const TestPool& test = kTestPool[i % base::size(kTestPool)];
    ui_test_utils::NavigateToURL(browser(),
                                 embedded_test_server()->GetURL(test.source));
    base::DelegateSimpleThread close_printdlg_thread1(&dismisser,
                                                      "close_printdlg_thread");
    EXPECT_EQ(NULL, FindDialogWindow(dismisser.owner_process()));
    close_printdlg_thread1.Start();
    PrintNowTab();
    close_printdlg_thread1.Join();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
    CleanupDumpDirectory();
    base::DelegateSimpleThread close_printdlg_thread2(&dismisser,
                                                      "close_printdlg_thread");
    EXPECT_EQ(NULL, FindDialogWindow(dismisser.owner_process()));
    close_printdlg_thread2.Start();
    PrintNowTab();
    close_printdlg_thread2.Join();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
    CleanupDumpDirectory();
    base::DelegateSimpleThread close_printdlg_thread3(&dismisser,
                                                      "close_printdlg_thread");
    EXPECT_EQ(NULL, FindDialogWindow(dismisser.owner_process()));
    close_printdlg_thread3.Start();
    PrintNowTab();
    close_printdlg_thread3.Join();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
    CleanupDumpDirectory();
    base::DelegateSimpleThread close_printdlg_thread4(&dismisser,
                                                      "close_printdlg_thread");
    EXPECT_EQ(NULL, FindDialogWindow(dismisser.owner_process()));
    close_printdlg_thread4.Start();
    PrintNowTab();
    close_printdlg_thread4.Join();
    EXPECT_EQ(0., CompareWithResult(test.result)) << test.result;
  }
}

// Prints a popup and immediately closes it. Disabled because it crashes.
IN_PROC_BROWSER_TEST_F(PrintingLayoutTest, DISABLED_Delayed) {
  if (IsTestCaseDisabled())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  {
    bool is_timeout = true;
    GURL url =
        embedded_test_server()->GetURL("/printing/popup_delayed_print.htm");
    ui_test_utils::NavigateToURL(browser(), url);

    DismissTheWindow dismisser;
    base::DelegateSimpleThread close_printdlg_thread(&dismisser,
                                                     "close_printdlg_thread");
    close_printdlg_thread.Start();
    close_printdlg_thread.Join();

    // Force a navigation elsewhere to verify that it's fine with it.
    url = embedded_test_server()->GetURL("/printing/test1.html");
    ui_test_utils::NavigateToURL(browser(), url);
  }
  chrome::CloseWindow(browser());
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(0., CompareWithResult(L"popup_delayed_print"))
      << L"popup_delayed_print";
}

// Prints a popup and immediately closes it. http://crbug.com/7721
IN_PROC_BROWSER_TEST_F(PrintingLayoutTest, DISABLED_IFrame) {
  if (IsTestCaseDisabled())
    return;

  ASSERT_TRUE(embedded_test_server()->Start());

  {
    GURL url = embedded_test_server()->GetURL("/printing/iframe.htm");
    ui_test_utils::NavigateToURL(browser(), url);

    DismissTheWindow dismisser;
    base::DelegateSimpleThread close_printdlg_thread(&dismisser,
                                                     "close_printdlg_thread");
    close_printdlg_thread.Start();
    close_printdlg_thread.Join();

    // Force a navigation elsewhere to verify that it's fine with it.
    url = embedded_test_server()->GetURL("/printing/test1.html");
    ui_test_utils::NavigateToURL(browser(), url);
  }
  chrome::CloseWindow(browser());
  content::RunAllPendingInMessageLoop();

  EXPECT_EQ(0., CompareWithResult(L"iframe")) << L"iframe";
}
