// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"

#include <tuple>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/mock_callback.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_error_dialog.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_warn_dialog.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/enterprise/data_controls/component.h"
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
  base::MockCallback<OnDlpRestrictionCheckedCallback> cb_;
};

// Tests that the warning dialog is created as a system modal if no parent is
// passed, and that accepting the dialog runs the callback with true.
IN_PROC_BROWSER_TEST_P(WarningDialogBrowserTest, NoParent) {
  dlp::FileAction action = GetParam();

  auto* widget =
      FilesPolicyDialog::CreateWarnDialog(cb_.Get(), warning_files_, action,
                                          /*modal_parent=*/nullptr);
  ASSERT_TRUE(widget);

  FilesPolicyWarnDialog* dialog = static_cast<FilesPolicyWarnDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  EXPECT_EQ(dialog->GetModalType(), ui::ModalType::MODAL_TYPE_SYSTEM);
  // Proceed.
  EXPECT_CALL(cb_, Run(/*should_proceed=*/true)).Times(1);
  dialog->AcceptDialog();
  EXPECT_TRUE(widget->IsClosed());
}

// Tests that the warning dialog is created as a window modal if a Files app
// window is passed as the parent, and that cancelling the dialog runs the
// callback with false.
IN_PROC_BROWSER_TEST_P(WarningDialogBrowserTest, WithParent) {
  dlp::FileAction action = GetParam();

  ASSERT_FALSE(FindFilesApp());
  Browser* files_app = OpenFilesApp();
  ASSERT_TRUE(files_app);
  ASSERT_EQ(files_app, FindFilesApp());

  auto* widget = FilesPolicyDialog::CreateWarnDialog(
      cb_.Get(), warning_files_, action,
      files_app->window()->GetNativeWindow());
  ASSERT_TRUE(widget);

  FilesPolicyWarnDialog* dialog = static_cast<FilesPolicyWarnDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  EXPECT_EQ(dialog->GetModalType(), ui::ModalType::MODAL_TYPE_WINDOW);
  EXPECT_EQ(widget->parent()->GetNativeWindow(),
            files_app->window()->GetNativeWindow());
  // Cancel.
  EXPECT_CALL(cb_, Run(/*should_proceed=*/false)).Times(1);
  dialog->CancelDialog();
  EXPECT_TRUE(widget->IsClosed());
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
// passed, and that accepting the dialog dismisses it without any other action.
IN_PROC_BROWSER_TEST_P(ErrorDialogBrowserTest, NoParent) {
  dlp::FileAction action = GetParam();
  // Add another blocked file to test the mixed error case.
  blocked_files_.emplace(DlpConfidentialFile(base::FilePath("file3.txt")),
                         Policy::kEnterpriseConnectors);

  auto* widget = FilesPolicyDialog::CreateErrorDialog(blocked_files_, action,
                                                      /*modal_parent=*/nullptr);
  ASSERT_TRUE(widget);

  FilesPolicyErrorDialog* dialog = static_cast<FilesPolicyErrorDialog*>(
      widget->widget_delegate()->AsDialogDelegate());
  ASSERT_TRUE(dialog);

  EXPECT_EQ(dialog->GetModalType(), ui::ModalType::MODAL_TYPE_SYSTEM);
  // Accept -> dismiss.
  dialog->AcceptDialog();
  EXPECT_TRUE(widget->IsClosed());
}

// Tests that the error dialog is created as a window modal if a Files app
// window is passed as the parent, and that cancelling the dialog opens the help
// article page.
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
  // Cancel -> Learn more.
  EXPECT_NE(
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().spec(),
      dlp::kDlpLearnMoreUrl);
  dialog->CancelDialog();
  EXPECT_TRUE(widget->IsClosed());
  EXPECT_EQ(
      browser()->tab_strip_model()->GetActiveWebContents()->GetURL().spec(),
      dlp::kDlpLearnMoreUrl);
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
      /*modal_parent=*/nullptr,
      DlpFileDestination(GURL("https://example.com"))));
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

class WarningComponentBrowserTest
    : public DlpWarningDialogDestinationBrowserTest,
      public ::testing::WithParamInterface<
          std::tuple<dlp::FileAction, DlpFileDestination>> {};

IN_PROC_BROWSER_TEST_P(WarningComponentBrowserTest, CreateDialog) {
  auto [action, destination] = GetParam();

  ASSERT_TRUE(FilesPolicyDialog::CreateWarnDialog(
      base::DoNothing(), warning_files_, action,
      /*modal_parent=*/nullptr, destination));
}

INSTANTIATE_TEST_SUITE_P(
    FilesPolicyDialog,
    WarningComponentBrowserTest,
    ::testing::Values(
        std::make_tuple(dlp::FileAction::kUpload,
                        DlpFileDestination(GURL("https://example.com"))),
        std::make_tuple(dlp::FileAction::kTransfer,
                        DlpFileDestination(data_controls::Component::kArc)),
        std::make_tuple(
            dlp::FileAction::kUnknown,
            DlpFileDestination(data_controls::Component::kCrostini)),
        std::make_tuple(dlp::FileAction::kOpen,
                        DlpFileDestination(data_controls::Component::kUsb)),
        std::make_tuple(
            dlp::FileAction::kMove,
            DlpFileDestination(data_controls::Component::kPluginVm)),
        std::make_tuple(
            dlp::FileAction::kShare,
            DlpFileDestination(data_controls::Component::kOneDrive))));

}  // namespace policy
