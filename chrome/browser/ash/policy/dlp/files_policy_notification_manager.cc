// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"

namespace policy {

FilesPolicyNotificationManager::FilesPolicyNotificationManager(
    content::BrowserContext* context) {
  DCHECK(context);

  context_ = context;
}

FilesPolicyNotificationManager::~FilesPolicyNotificationManager() = default;

void FilesPolicyNotificationManager::ShowDialog(
    file_manager::io_task::IOTaskId task_id,
    FilesDialogType type) {
  auto* profile = Profile::FromBrowserContext(context_);
  DCHECK(profile);

  // Get the last active Files app window.
  Browser* browser =
      FindSystemWebAppBrowser(profile, ash::SystemWebAppType::FILE_MANAGER);
  gfx::NativeWindow modal_parent =
      browser ? browser->window()->GetNativeWindow() : nullptr;

  if (modal_parent) {
    // TODO(b/282664769): Pass correct values. These should be stored by
    // task_id.
    ShowFilesPolicyDialog(base::DoNothing(), std::vector<DlpConfidentialFile>(),
                          DlpFileDestination(""),
                          DlpFilesController::FileAction::kCopy, modal_parent);
    return;
  }

  // No window found, so open a new one. This should notify us by through
  // OnBrowserSetLastActive() to show the dialog.
  BrowserList::AddObserver(this);
  DCHECK(!pending_callback_);
  // TODO(b/282664769): Bind correct values. These should be stored by task_id.
  pending_callback_ =
      base::BindOnce(&FilesPolicyNotificationManager::ShowFilesPolicyDialog,
                     weak_factory_.GetWeakPtr(), base::DoNothing(),
                     std::vector<DlpConfidentialFile>(), DlpFileDestination(""),
                     DlpFilesController::FileAction::kCopy);

  ui::SelectFileDialog::FileTypeInfo file_type_info;
  file_type_info.allowed_paths =
      ui::SelectFileDialog::FileTypeInfo::ANY_PATH_OR_URL;
  GURL files_swa_url = file_manager::util::GetFileManagerMainPageUrlWithParams(
      ui::SelectFileDialog::SELECT_NONE,
      /*title=*/{},
      /*current_directory_url=*/{},
      /*selection_url=*/{},
      /*target_name=*/{}, &file_type_info,
      /*file_type_index=*/0,
      /*search_query=*/{},
      /*show_android_picker_apps=*/false,
      /*volume_filter=*/{});
  ash::SystemAppLaunchParams params;
  params.url = files_swa_url;
  ash::LaunchSystemWebAppAsync(profile, ash::SystemWebAppType::FILE_MANAGER,
                               params);
}

void FilesPolicyNotificationManager::ShowFilesPolicyDialog(
    OnDlpRestrictionCheckedCallback callback,
    const std::vector<DlpConfidentialFile>& confidential_files,
    const DlpFileDestination& destination,
    DlpFilesController::FileAction action,
    gfx::NativeWindow modal_parent) {
  views::Widget* widget = views::DialogDelegate::CreateDialogWidget(
      std::make_unique<FilesPolicyDialog>(std::move(callback),
                                          confidential_files, destination,
                                          action, modal_parent),
      /*context=*/nullptr, /*parent=*/modal_parent);
  widget->Show();
  // TODO(ayaelattar): Timeout after total 5 minutes.
}

void FilesPolicyNotificationManager::OnBrowserSetLastActive(Browser* browser) {
  if (!ash::IsBrowserForSystemWebApp(browser,
                                     ash::SystemWebAppType::FILE_MANAGER)) {
    // TODO(b/282663949): Consider if we need a timeout here in case it never
    // opens.
    LOG(WARNING) << "Browser did not match Files app";
    return;
  }

  // Files app successfully opened.
  gfx::NativeWindow modal_parent = browser->window()->GetNativeWindow();

  BrowserList::RemoveObserver(this);

  DCHECK(pending_callback_);
  std::move(pending_callback_).Run(modal_parent);
}

}  // namespace policy
