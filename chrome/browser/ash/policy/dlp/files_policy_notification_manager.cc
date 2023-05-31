// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"
#include "chrome/browser/ash/file_manager/url_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/files_policy_error_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/files_policy_warn_dialog.h"
#include "chrome/browser/chromeos/policy/dlp/dialogs/policy_dialog_base.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_confidential_file.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_file_destination.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy {

namespace {
file_manager::io_task::IOTaskController* GetIOTaskController(
    content::BrowserContext* context) {
  DCHECK(context);
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(Profile::FromBrowserContext(context));
  if (!volume_manager) {
    LOG(ERROR) << "FilesPolicyNotificationManager failed to find "
                  "file_manager::VolumeManager";
    return nullptr;
  }
  return volume_manager->io_task_controller();
}
}  // namespace

FilesPolicyNotificationManager::FilesPolicyNotificationManager(
    content::BrowserContext* context)
    : context_(context) {
  DCHECK(context);

  auto* io_task_controller = GetIOTaskController(context_);
  if (!io_task_controller) {
    LOG(ERROR) << "FilesPolicyNotificationManager failed to find "
                  "file_manager::io_task::IOTaskController";
    return;
  }
  io_tasks_observation_.Observe(io_task_controller);
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
    ShowFilesPolicyDialog(task_id, type, modal_parent);
    return;
  }

  // No window found, so open a new one. This should notify us by through
  // OnBrowserSetLastActive() to show the dialog.
  BrowserList::AddObserver(this);
  DCHECK(!pending_callback_);
  pending_callback_ =
      base::BindOnce(&FilesPolicyNotificationManager::ShowFilesPolicyDialog,
                     weak_factory_.GetWeakPtr(), task_id, type);

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

bool FilesPolicyNotificationManager::HasIOTask(
    file_manager::io_task::IOTaskId task_id) const {
  return base::Contains(io_tasks_, task_id);
}

FilesPolicyNotificationManager::WarningInfo::WarningInfo(
    std::vector<base::FilePath> files_paths,
    Policy warning_reason)
    : warning_reason(warning_reason) {
  for (const auto& file_path : files_paths) {
    files.emplace_back(file_path);
  }
}

FilesPolicyNotificationManager::WarningInfo::WarningInfo(WarningInfo&& other) =
    default;

FilesPolicyNotificationManager::WarningInfo::~WarningInfo() = default;

FilesPolicyNotificationManager::IOTaskInfo::IOTaskInfo() = default;

FilesPolicyNotificationManager::IOTaskInfo::IOTaskInfo(IOTaskInfo&& other) =
    default;

FilesPolicyNotificationManager::IOTaskInfo::~IOTaskInfo() = default;

void FilesPolicyNotificationManager::ShowFilesPolicyDialog(
    file_manager::io_task::IOTaskId task_id,
    FilesDialogType type,
    gfx::NativeWindow modal_parent) {
  if (!HasIOTask(task_id)) {
    // Task already completed and removed.
    return;
  }

  views::Widget* widget;
  switch (type) {
    case FilesDialogType::kUnknown:
      LOG(WARNING) << "Unknown FilesDialogType passed";
      return;
    case FilesDialogType::kError:
      if (!io_tasks_.at(task_id).blocked_files.empty()) {
        return;
      }
      widget = views::DialogDelegate::CreateDialogWidget(
          std::make_unique<FilesPolicyErrorDialog>(
              std::move(io_tasks_.at(task_id).blocked_files),
              DlpFileDestination(""), io_tasks_.at(task_id).action,
              modal_parent),
          /*context=*/nullptr, /*parent=*/modal_parent);
      break;
    case FilesDialogType::kWarning:
      if (!io_tasks_.at(task_id).warning_info.has_value()) {
        return;
      }
      OnDlpRestrictionCheckedCallback callback = base::BindOnce(
          &FilesPolicyNotificationManager::OnWarningDialogClicked,
          weak_factory_.GetWeakPtr(), task_id,
          io_tasks_.at(task_id).warning_info->warning_reason);
      widget = views::DialogDelegate::CreateDialogWidget(
          std::make_unique<FilesPolicyWarnDialog>(
              std::move(callback),
              std::move(io_tasks_.at(task_id).warning_info->files),
              DlpFileDestination(""), io_tasks_.at(task_id).action,
              modal_parent),
          /*context=*/nullptr, /*parent=*/modal_parent);
      break;
  }
  widget->Show();
  // TODO(ayaelattar): Timeout after total 5 minutes.
}

void FilesPolicyNotificationManager::AddIOTask(
    file_manager::io_task::IOTaskId task_id) {
  io_tasks_.emplace(std::move(task_id), IOTaskInfo());
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

void FilesPolicyNotificationManager::OnIOTaskStatus(
    const file_manager::io_task::ProgressStatus& status) {
  // Observe only Copy and Move tasks.
  if (status.type != file_manager::io_task::OperationType::kCopy &&
      status.type != file_manager::io_task::OperationType::kMove) {
    return;
  }

  if (!HasIOTask(status.task_id) &&
      status.state == file_manager::io_task::State::kQueued) {
    AddIOTask(status.task_id);
  } else if (HasIOTask(status.task_id) && status.IsCompleted()) {
    // If it's in a terminal state, stop observing.
    io_tasks_.erase(status.task_id);
  }
}

bool FilesPolicyNotificationManager::HasBlockedFiles(
    file_manager::io_task::IOTaskId task_id) const {
  return HasIOTask(task_id) && !io_tasks_.at(task_id).blocked_files.empty();
}

void FilesPolicyNotificationManager::OnWarningDialogClicked(
    file_manager::io_task::IOTaskId task_id,
    Policy warning_reason,
    bool should_proceed) {
  auto* io_task_controller = GetIOTaskController(context_);
  if (!io_task_controller) {
    LOG(ERROR) << "FilesPolicyNotificationManager failed to find "
                  "file_manager::io_task::IOTaskController";
    return;
  }

  if (should_proceed) {
    file_manager::io_task::ResumeParams params;
    params.policy_params->type = warning_reason;
    io_task_controller->Resume(task_id, std::move(params));
  } else {
    io_task_controller->Cancel(task_id);
  }
}

}  // namespace policy
