// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"

#include <cstddef>
#include <string>

#include "ash/public/cpp/new_window_delegate.h"
#include "base/check.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/string_number_conversions.h"
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
#include "chromeos/dbus/dlp/dlp_service.pb.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/message_center/public/cpp/notification.h"

namespace policy {

// The policy notification button index.
enum NotificationButton {
  CANCEL = 0,
  OK,
};

namespace {

constexpr char kUploadBlockedNotificationId[] = "upload_dlp_blocked";
constexpr char kDownloadBlockedNotificationId[] = "download_dlp_blocked";
constexpr char kOpenBlockedNotificationId[] = "open_dlp_blocked";

// TODO(b/279435843): Replace with translation strings.
std::u16string GetNotificationTitle(
    const file_manager::io_task::ProgressStatus& status) {
  DCHECK(status.type == file_manager::io_task::OperationType::kCopy ||
         status.type == file_manager::io_task::OperationType::kMove);
  if (status.type == file_manager::io_task::OperationType::kCopy) {
    return status.HasWarning() ? u"Review is required before copying"
                               : u"Blocked copy";
  } else {  // kMove
    return status.HasWarning() ? u"Review is required before moving"
                               : u"Blocked move";
  }
}

// TODO(b/279435843): Replace with translation strings.
std::u16string GetNotificationMessage(
    const file_manager::io_task::ProgressStatus& status) {
  DCHECK(status.type == file_manager::io_task::OperationType::kCopy ||
         status.type == file_manager::io_task::OperationType::kMove);
  if (status.HasWarning()) {
    // TODO(): Use # warned files, not total.
    return status.sources.size() == 1 ? u"File may contain sensitive content"
                                      : u"Files may contain sensitive content";

  } else {  // Error
            // TODO(): Use # blocked files, not total.
    return status.sources.size() == 1 ? u"File was blocked"
                                      : u"Review for further details";
  }
}

// TODO(b/279435843): Replace with translation strings.
std::u16string GetOkButton(
    const file_manager::io_task::ProgressStatus& status) {
  DCHECK(status.type == file_manager::io_task::OperationType::kCopy ||
         status.type == file_manager::io_task::OperationType::kMove);
  // Multiple files - both warnings and errors have a Review button.
  // TODO(): Use # warned/blocked files, not total.
  if (status.sources.size() > 1) {
    return u"Review";
  }
  // Single file - button text depends on the type.
  if (status.HasWarning()) {
    return status.type == file_manager::io_task::OperationType::kCopy
               ? l10n_util::GetStringUTF16(
                     IDS_POLICY_DLP_FILES_COPY_WARN_CONTINUE_BUTTON)
               : l10n_util::GetStringUTF16(
                     IDS_POLICY_DLP_FILES_MOVE_WARN_CONTINUE_BUTTON);
  } else {  // Error
    return l10n_util::GetStringUTF16(IDS_LEARN_MORE);
  }
}

// TODO(b/279435843): Replace with translation strings.
std::u16string GetCancelButton(
    const file_manager::io_task::ProgressStatus& status) {
  DCHECK(status.type == file_manager::io_task::OperationType::kCopy ||
         status.type == file_manager::io_task::OperationType::kMove);
  return status.HasWarning()
             ? l10n_util::GetStringUTF16(IDS_POLICY_DLP_WARN_CANCEL_BUTTON)
             : u"Dismiss";
}

// Dismisses the notification with `notification_id`.
void Dismiss(content::BrowserContext* context,
             const std::string& notification_id) {
  auto* profile = Profile::FromBrowserContext(context);
  DCHECK(profile);
  NotificationDisplayServiceFactory::GetForProfile(profile)->Close(
      NotificationHandler::Type::TRANSIENT, notification_id);
}

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

// Computes and returns a new notification ID for `action`.
std::string GetNotificationId(dlp::FileAction action, size_t count) {
  switch (action) {
    case dlp::FileAction::kDownload:
      return kDownloadBlockedNotificationId + std::string("_") +
             base::NumberToString(count);
    case dlp::FileAction::kUpload:
      return kUploadBlockedNotificationId + std::string("_") +
             base::NumberToString(count);
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
      return kOpenBlockedNotificationId + std::string("_") +
             base::NumberToString(count);
    case dlp::FileAction::kCopy:
    case dlp::FileAction::kMove:
    case dlp::FileAction::kTransfer:
    case dlp::FileAction::kUnknown:
      // TODO(b/269609831): Return valid ID.
      return "";
  }
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

void FilesPolicyNotificationManager::ShowDlpBlockedFiles(
    absl::optional<file_manager::io_task::IOTaskId> task_id,
    std::vector<base::FilePath> blocked_files,
    dlp::FileAction action) {
  // If `task_id` has value, the corresponding IOTask should be updated
  // accordingly.
  if (task_id.has_value()) {
    if (!HasIOTask(task_id.value())) {
      // Task already completed and removed.
      return;
    }
    for (const auto& file : blocked_files) {
      io_tasks_.at(task_id.value())
          .blocked_files.emplace(DlpConfidentialFile(file), Policy::kDlp);
    }
  } else {
    ShowDlpBlockNotification(std::move(blocked_files), action);
  }
}

void FilesPolicyNotificationManager::ShowDlpWarning(
    OnDlpRestrictionCheckedCallback callback,
    absl::optional<file_manager::io_task::IOTaskId> task_id,
    std::vector<base::FilePath> warning_files,
    const DlpFileDestination& destination,
    dlp::FileAction action) {
  // If `task_id` has value, the corresponding IOTask should be paused.
  if (task_id.has_value()) {
    PauseIOTask(task_id.value(), std::move(callback), std::move(warning_files),
                action, Policy::kDlp);
  } else {
    ShowDlpWarningNotification(std::move(callback), std::move(warning_files),
                               destination, action);
  }
}

void FilesPolicyNotificationManager::ShowsFilesPolicyNotification(
    const std::string& notification_id,
    const file_manager::io_task::ProgressStatus& status) {
  file_manager::io_task::IOTaskId id(status.task_id);
  auto callback =
      status.HasWarning()
          ? base::BindRepeating(&FilesPolicyNotificationManager::
                                    HandleFilesPolicyWarningNotificationClick,
                                weak_factory_.GetWeakPtr(), id, notification_id)
          : base::BindRepeating(&FilesPolicyNotificationManager::
                                    HandleFilesPolicyErrorNotificationClick,
                                weak_factory_.GetWeakPtr(), id,
                                notification_id);
  // The notification should stay visible until actioned upon.
  message_center::RichNotificationData optional_fields;
  optional_fields.never_timeout = true;
  auto notification = file_manager::CreateSystemNotification(
      notification_id, GetNotificationTitle(status),
      GetNotificationMessage(status),
      base::MakeRefCounted<message_center::HandleNotificationClickDelegate>(
          std::move(callback)),
      optional_fields);
  notification->set_buttons(
      {message_center::ButtonInfo(GetCancelButton(status)),
       message_center::ButtonInfo(GetOkButton(status))});
  auto* profile = Profile::FromBrowserContext(context_);
  DCHECK(profile);
  NotificationDisplayServiceFactory::GetForProfile(profile)->Display(
      NotificationHandler::Type::TRANSIENT, *notification,
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

void FilesPolicyNotificationManager::ResumeIOTask(
    file_manager::io_task::IOTaskId task_id) {
  if (!HasIOTask(task_id)) {
    // Task is already completed or timed out.
    return;
  }

  if (!HasWarning(task_id)) {
    // Warning callback is already run.
    return;
  }

  std::move(io_tasks_.at(task_id).warning_info->warning_callback)
      .Run(/*should_proceed=*/true);
  io_tasks_.at(task_id).warning_info.reset();
}

std::map<DlpConfidentialFile, Policy>
FilesPolicyNotificationManager::GetIOTaskBlockedFilesForTesting(
    file_manager::io_task::IOTaskId task_id) const {
  if (!HasIOTask(task_id)) {
    return {};
  }
  return io_tasks_.at(task_id).blocked_files;
}

FilesPolicyNotificationManager::WarningInfo::WarningInfo(
    std::vector<base::FilePath> files_paths,
    Policy warning_reason,
    OnDlpRestrictionCheckedCallback warning_callback)
    : warning_reason(warning_reason),
      warning_callback(std::move(warning_callback)) {
  for (const auto& file_path : files_paths) {
    files.emplace_back(file_path);
  }
}

FilesPolicyNotificationManager::WarningInfo::WarningInfo(WarningInfo&& other) =
    default;

FilesPolicyNotificationManager::WarningInfo::~WarningInfo() = default;

FilesPolicyNotificationManager::FileTaskInfo::FileTaskInfo(
    dlp::FileAction action)
    : action(action) {}

FilesPolicyNotificationManager::FileTaskInfo::FileTaskInfo(
    FileTaskInfo&& other) = default;

FilesPolicyNotificationManager::FileTaskInfo::~FileTaskInfo() = default;

void FilesPolicyNotificationManager::HandleFilesPolicyWarningNotificationClick(
    file_manager::io_task::IOTaskId task_id,
    std::string notification_id,
    absl::optional<int> button_index) {
  if (!button_index.has_value()) {
    return;
  }
  if (!HasIOTask(task_id)) {
    // Task already completed.
    return;
  }
  if (!HasWarning(task_id)) {
    LOG(WARNING) << "Warning notification clicked but no warning info found";
    return;
  }

  if (button_index.value() == NotificationButton::CANCEL) {
    Cancel(task_id);
    Dismiss(context_, notification_id);
  }

  if (button_index.value() == NotificationButton::OK) {
    if (io_tasks_.at(task_id).warning_info->files.size() == 1) {
      // Single file - proceed.
      Resume(task_id);
    } else {
      // Multiple files - review.
      ShowDialog(task_id, FilesDialogType::kWarning);
    }
    Dismiss(context_, notification_id);
  }
}

void FilesPolicyNotificationManager::HandleFilesPolicyErrorNotificationClick(
    file_manager::io_task::IOTaskId task_id,
    std::string notification_id,
    absl::optional<int> button_index) {
  if (!button_index.has_value()) {
    return;
  }
  if (!HasIOTask(task_id)) {
    // Task already completed.
    return;
  }
  if (!HasBlockedFiles(task_id)) {
    LOG(WARNING) << "Error notification clicked but no blocked files found";
    return;
  }

  if (button_index.value() == NotificationButton::CANCEL) {
    Dismiss(context_, notification_id);
  }

  if (button_index.value() == NotificationButton::OK) {
    if (io_tasks_.at(task_id).blocked_files.size() == 1) {
      // Single file - open help page.
      // TODO(b/283786134): Open page based on policy.
      ash::NewWindowDelegate::GetPrimary()->OpenUrl(
          GURL(dlp::kDlpLearnMoreUrl),
          ash::NewWindowDelegate::OpenUrlFrom::kUserInteraction,
          ash::NewWindowDelegate::Disposition::kNewForegroundTab);
    } else {
      // Multiple files - review.
      ShowDialog(task_id, FilesDialogType::kError);
    }
  }
  Dismiss(context_, notification_id);
}

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
      if (!HasBlockedFiles(task_id)) {
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
      if (!HasWarning(task_id)) {
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
    file_manager::io_task::IOTaskId task_id,
    dlp::FileAction action) {
  io_tasks_.emplace(std::move(task_id), FileTaskInfo(action));
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

  dlp::FileAction action =
      status.type == file_manager::io_task::OperationType::kCopy
          ? dlp::FileAction::kCopy
          : dlp::FileAction::kMove;

  if (!HasIOTask(status.task_id) &&
      status.state == file_manager::io_task::State::kQueued) {
    AddIOTask(status.task_id, action);
  } else if (HasIOTask(status.task_id) && status.IsCompleted()) {
    if (status.state == file_manager::io_task::State::kCancelled &&
        HasWarning(status.task_id)) {
      CHECK(!io_tasks_.at(status.task_id)
                 .warning_info->warning_callback.is_null());
      std::move(io_tasks_.at(status.task_id).warning_info->warning_callback)
          .Run(/*should_proceed=*/false);
      io_tasks_.at(status.task_id).warning_info.reset();
    }
    // If it's in a terminal state, stop observing.
    io_tasks_.erase(status.task_id);
  }
}

bool FilesPolicyNotificationManager::HasBlockedFiles(
    file_manager::io_task::IOTaskId task_id) const {
  return HasIOTask(task_id) && !io_tasks_.at(task_id).blocked_files.empty();
}

bool FilesPolicyNotificationManager::HasWarning(
    file_manager::io_task::IOTaskId task_id) const {
  return HasIOTask(task_id) && io_tasks_.at(task_id).warning_info.has_value();
}

void FilesPolicyNotificationManager::OnWarningDialogClicked(
    file_manager::io_task::IOTaskId task_id,
    Policy warning_reason,
    bool should_proceed) {
  if (!HasIOTask(task_id) || !HasWarning(task_id)) {
    // Task probably timed out.
    return;
  }

  if (should_proceed) {
    Resume(task_id);
  } else {
    Cancel(task_id);
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

void FilesPolicyNotificationManager::Resume(
    file_manager::io_task::IOTaskId task_id) {
  if (!HasIOTask(task_id) || !HasWarning(task_id)) {
    return;
  }
  auto* io_task_controller = GetIOTaskController(context_);
  if (!io_task_controller) {
    LOG(ERROR) << "FilesPolicyNotificationManager failed to find "
                  "file_manager::io_task::IOTaskController";
    return;
  }
  file_manager::io_task::ResumeParams params;
  params.policy_params->type =
      io_tasks_.at(task_id).warning_info->warning_reason;
  io_task_controller->Resume(task_id, std::move(params));
}

void FilesPolicyNotificationManager::Cancel(
    file_manager::io_task::IOTaskId task_id) {
  if (!HasIOTask(task_id) || !HasWarning(task_id)) {
    return;
  }
  auto* io_task_controller = GetIOTaskController(context_);
  if (!io_task_controller) {
    LOG(ERROR) << "FilesPolicyNotificationManager failed to find "
                  "file_manager::io_task::IOTaskController";
    return;
  }
  io_task_controller->Cancel(task_id);
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

void FilesPolicyNotificationManager::ShowDlpBlockNotification(
    std::vector<base::FilePath> blocked_files,
    dlp::FileAction action) {
  std::u16string title;
  std::u16string message;

  switch (action) {
    case dlp::FileAction::kDownload:
      title =
          l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_DOWNLOAD_BLOCK_TITLE);
      // ignore `blocked_files.size()` for downloads.
      message = l10n_util::GetStringUTF16(
          IDS_POLICY_DLP_FILES_DOWNLOAD_BLOCK_MESSAGE);
      break;
    case dlp::FileAction::kUpload:
      title =
          l10n_util::GetStringUTF16(IDS_POLICY_DLP_FILES_UPLOAD_BLOCK_TITLE);
      message = l10n_util::GetPluralStringFUTF16(
          IDS_POLICY_DLP_FILES_UPLOAD_BLOCK_MESSAGE, blocked_files.size());
      break;
    case dlp::FileAction::kOpen:
    case dlp::FileAction::kShare:
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
  const std::string notification_id =
      GetNotificationId(action, notification_count_++);
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

void FilesPolicyNotificationManager::ShowDlpWarningNotification(
    OnDlpRestrictionCheckedCallback callback,
    std::vector<base::FilePath> warning_files,
    const DlpFileDestination& destination,
    dlp::FileAction action) {
  auto* widget = views::DialogDelegate::CreateDialogWidget(
      std::make_unique<FilesPolicyWarnDialog>(
          std::move(callback),
          std::vector<DlpConfidentialFile>{warning_files.begin(),
                                           warning_files.end()},
          destination, action,
          /*modal_parent=*/nullptr),
      /*context=*/nullptr,
      /*parent=*/nullptr);
  widget->Show();
  // TODO(ayaelattar): Timeout after total 5 minutes.
}

void FilesPolicyNotificationManager::PauseIOTask(
    file_manager::io_task::IOTaskId task_id,
    OnDlpRestrictionCheckedCallback callback,
    std::vector<base::FilePath> warning_files,
    dlp::FileAction action,
    Policy warning_reason) {
  auto* io_task_controller = GetIOTaskController(context_);
  if (!io_task_controller || !HasIOTask(task_id)) {
    // Proceed because the IO task can't be paused.
    std::move(callback).Run(/*should_proceed=*/true);
    return;
  }

  io_tasks_.at(task_id).warning_info.emplace(
      std::move(warning_files), warning_reason, std::move(callback));

  file_manager::io_task::PauseParams pause_params;
  pause_params.policy_params =
      file_manager::io_task::PolicyPauseParams(Policy::kDlp);
  // TODO(b/285880274): Pass number of files on PolicyPauseParams because it's
  // needed for the strings.
  io_task_controller->Pause(task_id, std::move(pause_params));
  // TODO(ayaelattar): Timeout after total 5 minutes.
}

}  // namespace policy
