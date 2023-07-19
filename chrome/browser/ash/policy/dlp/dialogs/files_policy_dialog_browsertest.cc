// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_warn_dialog.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/test/browser_test.h"
#include "ui/base/ui_base_types.h"

namespace policy {
class FilesPolicyDialogBrowserTest
    : public InProcessBrowserTest,
      public ::testing::WithParamInterface<dlp::FileAction> {
 public:
  FilesPolicyDialogBrowserTest() = default;
  FilesPolicyDialogBrowserTest(const FilesPolicyDialogBrowserTest&) = delete;
  FilesPolicyDialogBrowserTest& operator=(const FilesPolicyDialogBrowserTest&) =
      delete;
  ~FilesPolicyDialogBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    DlpFilesController::SetNewFilesPolicyUXEnabledForTesting(
        /*is_enabled=*/true);

    // Setup the Files app.
    ash::SystemWebAppManager::GetForTest(browser()->profile())
        ->InstallSystemAppsForTesting();
    file_manager::test::AddDefaultComponentExtensionsOnMainThread(
        browser()->profile());
  }

 protected:
  Browser* FindFilesApp() {
    return FindSystemWebAppBrowser(browser()->profile(),
                                   ash::SystemWebAppType::FILE_MANAGER);
  }

  Browser* OpenFilesApp() {
    base::RunLoop run_loop;
    file_manager::util::ShowItemInFolder(
        browser()->profile(),
        file_manager::util::GetDownloadsFolderForProfile(browser()->profile()),
        base::BindLambdaForTesting(
            [&run_loop](platform_util::OpenOperationResult result) {
              EXPECT_EQ(platform_util::OpenOperationResult::OPEN_SUCCEEDED,
                        result);
              run_loop.Quit();
            }));
    run_loop.Run();
    return ui_test_utils::WaitForBrowserToOpen();
  }
};

class WarningDialogBrowserTest : public FilesPolicyDialogBrowserTest {
 public:
  void SetUpOnMainThread() override {
    FilesPolicyDialogBrowserTest::SetUpOnMainThread();

    warning_files_.emplace_back(base::FilePath("file1.txt"));
    warning_files_.emplace_back(base::FilePath("file2.txt"));
  }

 protected:
  std::vector<DlpConfidentialFile> warning_files_;
};

// Tests that the warning dialog is created as a system modal if no parent is
// passed.
IN_PROC_BROWSER_TEST_P(WarningDialogBrowserTest, NoParent) {
  dlp::FileAction action = GetParam();

  auto* widget = FilesPolicyDialog::CreateWarnDialog(base::DoNothing(),
                                                     warning_files_, action,
                                                     /*modal_parent=*/nullptr);
  ASSERT_TRUE(widget);

  FilesPolicyWarnDialog* dialog = static_cast<FilesPolicyWarnDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  EXPECT_EQ(dialog->GetModalType(), ui::ModalType::MODAL_TYPE_SYSTEM);
}

// Tests that the warning dialog is created as a window modal if a Files app
// window is passed as the parent.
IN_PROC_BROWSER_TEST_P(WarningDialogBrowserTest, WithParent) {
  dlp::FileAction action = GetParam();

  ASSERT_FALSE(FindFilesApp());
  Browser* files_app = OpenFilesApp();
  ASSERT_TRUE(files_app);
  ASSERT_EQ(files_app, FindFilesApp());

  auto* widget = FilesPolicyDialog::CreateWarnDialog(
      base::DoNothing(), warning_files_, action,
      files_app->window()->GetNativeWindow());
  ASSERT_TRUE(widget);

  FilesPolicyWarnDialog* dialog = static_cast<FilesPolicyWarnDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  EXPECT_EQ(dialog->GetModalType(), ui::ModalType::MODAL_TYPE_WINDOW);
  EXPECT_EQ(widget->parent()->GetNativeWindow(),
            files_app->window()->GetNativeWindow());
}

INSTANTIATE_TEST_SUITE_P(FilesPolicyDialog,
                         WarningDialogBrowserTest,
                         ::testing::Values(dlp::FileAction::kDownload,
                                           dlp::FileAction::kTransfer,
                                           dlp::FileAction::kUpload,
                                           dlp::FileAction::kCopy,
                                           dlp::FileAction::kMove,
                                           dlp::FileAction::kOpen,
                                           dlp::FileAction::kShare));

class ErrorDialogBrowserTest : public FilesPolicyDialogBrowserTest {
 public:
  void SetUpOnMainThread() override {
    FilesPolicyDialogBrowserTest::SetUpOnMainThread();

    blocked_files_.emplace(DlpConfidentialFile(base::FilePath("file1.txt")),
                           Policy::kDlp);
    blocked_files_.emplace(DlpConfidentialFile(base::FilePath("file2.txt")),
                           Policy::kDlp);
  }

 protected:
  std::map<DlpConfidentialFile, Policy> blocked_files_;
};

// Tests that the error dialog is created as a system modal if no parent is
// passed.
IN_PROC_BROWSER_TEST_P(ErrorDialogBrowserTest, NoParent) {
  dlp::FileAction action = GetParam();
  auto* widget = FilesPolicyDialog::CreateErrorDialog(blocked_files_, action,
                                                      /*modal_parent=*/nullptr);
  ASSERT_TRUE(widget);

  FilesPolicyErrorDialog* dialog = static_cast<FilesPolicyErrorDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  EXPECT_EQ(dialog->GetModalType(), ui::ModalType::MODAL_TYPE_SYSTEM);
}

// Tests that the warning dialog is created as a window modal if a Files app
// window is passed as the parent.
IN_PROC_BROWSER_TEST_P(ErrorDialogBrowserTest, WithParent) {
  dlp::FileAction action = GetParam();

  ASSERT_FALSE(FindFilesApp());
  Browser* files_app = OpenFilesApp();
  ASSERT_TRUE(files_app);
  ASSERT_EQ(files_app, FindFilesApp());

  auto* widget = FilesPolicyDialog::CreateErrorDialog(
      blocked_files_, action, files_app->window()->GetNativeWindow());
  ASSERT_TRUE(widget);

  FilesPolicyErrorDialog* dialog = static_cast<FilesPolicyErrorDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  EXPECT_EQ(dialog->GetModalType(), ui::ModalType::MODAL_TYPE_WINDOW);
  EXPECT_EQ(widget->parent()->GetNativeWindow(),
            files_app->window()->GetNativeWindow());
}

INSTANTIATE_TEST_SUITE_P(FilesPolicyDialog,
                         ErrorDialogBrowserTest,
                         ::testing::Values(dlp::FileAction::kDownload,
                                           dlp::FileAction::kTransfer,
                                           dlp::FileAction::kUpload,
                                           dlp::FileAction::kCopy,
                                           dlp::FileAction::kMove,
                                           dlp::FileAction::kOpen,
                                           dlp::FileAction::kShare));

// Class to test "old" DLP Files restriction warning dialogs.
class DlpWarningDialogDestinationBrowserTest : public InProcessBrowserTest {
 public:
  DlpWarningDialogDestinationBrowserTest() = default;
  DlpWarningDialogDestinationBrowserTest(
      const DlpWarningDialogDestinationBrowserTest&) = delete;
  DlpWarningDialogDestinationBrowserTest& operator=(
      const DlpWarningDialogDestinationBrowserTest&) = delete;
  ~DlpWarningDialogDestinationBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    warning_files_.emplace_back(base::FilePath("file1.txt"));
    warning_files_.emplace_back(base::FilePath("file2.txt"));
  }

 protected:
  std::vector<DlpConfidentialFile> warning_files_;
};

// (b/273269211): This is a test for the crash that happens upon showing a
// warning dialog when a file is moved to Google Drive.
IN_PROC_BROWSER_TEST_F(DlpWarningDialogDestinationBrowserTest,
                       ComponentDestination) {
  ASSERT_TRUE(FilesPolicyDialog::CreateWarnDialog(
      base::DoNothing(), warning_files_, dlp::FileAction::kMove,
      /*modal_parent=*/nullptr,
      DlpFileDestination(data_controls::Component::kDrive)));
}

// (b/277594200): This is a test for the crash that happens upon showing a
// warning dialog when a file is dragged to a webpage.
IN_PROC_BROWSER_TEST_F(DlpWarningDialogDestinationBrowserTest, UrlDestination) {
  ASSERT_TRUE(FilesPolicyDialog::CreateWarnDialog(
      base::DoNothing(), warning_files_, dlp::FileAction::kCopy,
      /*modal_parent=*/nullptr, DlpFileDestination("htpps://example.com")));
}

// (b/281495499): This is a test for the crash that happens upon showing a
// warning dialog for downloads.
IN_PROC_BROWSER_TEST_F(DlpWarningDialogDestinationBrowserTest, Download) {
  ASSERT_TRUE(FilesPolicyDialog::CreateWarnDialog(
      base::DoNothing(),
      std::vector<DlpConfidentialFile>{
          DlpConfidentialFile(base::FilePath("file1.txt"))},
      dlp::FileAction::kDownload,
      /*modal_parent=*/nullptr,
      DlpFileDestination(data_controls::Component::kDrive)));
}

}  // namespace policy
