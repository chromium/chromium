// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/file_manager/file_manager_test_util.h"
#include "chrome/browser/ash/file_manager/open_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
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

    warning_files_.emplace_back(base::FilePath("file1.txt"));
    warning_files_.emplace_back(base::FilePath("file2.txt"));
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

  std::vector<DlpConfidentialFile> warning_files_;
};

// Tests that the warning dialog is created as a system modal if no parent is
// passed.
IN_PROC_BROWSER_TEST_P(FilesPolicyDialogBrowserTest, Warning_NoParent) {
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
IN_PROC_BROWSER_TEST_P(FilesPolicyDialogBrowserTest, Warning_WithParent) {
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
                         FilesPolicyDialogBrowserTest,
                         ::testing::Values(dlp::FileAction::kDownload,
                                           dlp::FileAction::kTransfer,
                                           dlp::FileAction::kUpload,
                                           dlp::FileAction::kCopy,
                                           dlp::FileAction::kMove,
                                           dlp::FileAction::kOpen,
                                           dlp::FileAction::kShare));

}  // namespace policy
