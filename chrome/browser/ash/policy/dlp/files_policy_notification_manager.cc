// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include "ash/public/cpp/new_window_delegate.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/ash/extensions/file_manager/system_notification_manager.h"
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
#include "chrome/browser/chromeos/policy/dlp/dlp_policy_constants.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/notifications/notification_display_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy {
namespace {

constexpr char kUploadBlockedNotificationId[] = "upload_dlp_blocked";
constexpr char kDownloadBlockedNotificationId[] = "download_dlp_blocked";
constexpr char kOpenBlockedNotificationId[] = "open_dlp_blocked";

}  // namespace

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
  io_task_controller->AddObserver(this);
}

FilesPolicyNotificationManager::~FilesPolicyNotificationManager() = default;

void FilesPolicyNotificationManager::ShowDlpWarning(
    OnDlpRestrictionCheckedCallback callback,
    const std::vector<DlpConfidentialFile>& confidential_files,
    const DlpFileDestination& destination,
    dlp::FileAction action) {
  auto* widget = views::DialogDelegate::CreateDialogWidget(
      std::make_unique<FilesPolicyWarnDialog>(std::move(callback),
                                              std::move(confidential_files),
                                              destination, action,
                                              /*modal_parent=*/nullptr),
      /*context=*/nullptr,
      /*parent=*/nullptr);
  widget->Show();
  // TODO(ayaelattar): Timeout after total 5 minutes.
}

void FilesPolicyNotificationManager::ShowDlpBlockNotification(
    dlp::FileAction action,
    const std::vector<base::FilePath>& blocked_files) {
  std::string notification_id;
  std::u16string title;
  std::u16string message;

  switch (action) {
    case dlp::FileAction::kDownload:
      notification_id = kDownloadBlockedNotificationId,
      title =
          l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_DOWNLOAD_BLOCK_TITLE);
      // ignore `blocked_files.size()` for downloads.
      message = l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DOWNLOAD_BLOCK_MESSAGE);
      break;
    case dlp::FileAction::kUpload:
      notification_id = kUploadBlockedNotificationId,
      title =
          l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_UPLOAD_BLOCK_TITLE);
      message = l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_BLOCK_MESSAGE, blocked_files.size());
      break;
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      notification_id = kOpenBlockedNotificationId,
      title = l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_OPEN_BLOCK_TITLE);
      message = l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_OPEN_BLOCK_MESSAGE, blocked_files.size());
      break;
    case dlp::FileAction::kCopy:
    case dlp::FileAction::kMove:
    case dlp::FileAction::kTransfer:
    case dlp::FileAction::kUnknown:
      // TODO(b/269609831): Show correct notification here.
      return;
  }
  auto notification = file_manager::CreateSystemNotification(
      notification_id, std::move(title), std::move(message),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          base::BindRepeating(
              &FilesPolicyNotificationManager::OnLearnMoreButtonClicked,
              weak_factory_.GetWeakPtr(), notification_id)));
  notification->set_buttons(
      {message_center::ButtonInfo(l10n_util::GetStringUTF16(IDS_LEARN_MORE))});

  NotificationDisplayServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context_))
      ->Display(NotificationHandler::Type::TRANSIENT, *notification,
                /*metadata=*/nullptr);
}

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

void FilesPolicyNotificationManager::OnLearnMoreButtonClicked(
    const std::string& notification_id,
    absl::optional<int> button_index) {
  if (!button_index || button_index.value() != 0) {
    return;
  }

  ash::NewWindowDelegate::GetPrimary()->OpenUrl(
      GURL(dlp::kDlpLearnMoreUrl),
      ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
      ash::NewWindowDelegate::Disposition::kNewForegroundTab);

  NotificationDisplayServiceFactory::GetForProfile(
      Profile::FromBrowserContext(context_))
      ->Close(NotificationHandler::Type::TRANSIENT, notification_id);
}

void FilesPolicyNotificationManager::Shutdown() {
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(Profile::FromBrowserContext(context_));
  if (volume_manager) {
    auto* io_task_controller = volume_manager->io_task_controller();
    if (io_task_controller) {
      io_task_controller->RemoveObserver(this);
    }
  }
}

}  // namespace policy
