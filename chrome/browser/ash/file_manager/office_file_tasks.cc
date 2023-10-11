// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/office_file_tasks.h"

#include <initializer_list>

#include "base/metrics/histogram_macros.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/app_id.h"
#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_open_metrics.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_ui.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "chrome/common/pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "content/public/browser/network_service_instance.h"
#include "extensions/common/constants.h"

namespace file_manager::file_tasks {

namespace {

// The map with pairs Office file extensions with their corresponding
// `OfficeOpenExtensions` enum.
constexpr auto kExtensionToOfficeOpenExtensionsEnum =
    base::MakeFixedFlatMap<base::StringPiece, OfficeOpenExtensions>(
        {{".doc", OfficeOpenExtensions::kDoc},
         {".docm", OfficeOpenExtensions::kDocm},
         {".docx", OfficeOpenExtensions::kDocx},
         {".dotm", OfficeOpenExtensions::kDotm},
         {".dotx", OfficeOpenExtensions::kDotx},
         {".odp", OfficeOpenExtensions::kOdp},
         {".ods", OfficeOpenExtensions::kOds},
         {".odt", OfficeOpenExtensions::kOdt},
         {".pot", OfficeOpenExtensions::kPot},
         {".potm", OfficeOpenExtensions::kPotm},
         {".potx", OfficeOpenExtensions::kPotx},
         {".ppam", OfficeOpenExtensions::kPpam},
         {".pps", OfficeOpenExtensions::kPps},
         {".ppsm", OfficeOpenExtensions::kPpsm},
         {".ppsx", OfficeOpenExtensions::kPpsx},
         {".ppt", OfficeOpenExtensions::kPpt},
         {".pptm", OfficeOpenExtensions::kPptm},
         {".pptx", OfficeOpenExtensions::kPptx},
         {".xls", OfficeOpenExtensions::kXls},
         {".xlsb", OfficeOpenExtensions::kXlsb},
         {".xlsm", OfficeOpenExtensions::kXlsm},
         {".xlsx", OfficeOpenExtensions::kXlsx}});

// Returns True if the `app_id` belongs to Files app either extension or SWA.
inline bool IsFilesAppId(const std::string& app_id) {
  return app_id == kFileManagerAppId || app_id == kFileManagerSwaAppId;
}

OfficeOpenExtensions GetOfficeOpenExtension(const storage::FileSystemURL& url) {
  const std::string extension = base::ToLowerASCII(url.path().FinalExtension());
  auto* itr = kExtensionToOfficeOpenExtensionsEnum.find(extension);
  if (itr != kExtensionToOfficeOpenExtensionsEnum.end()) {
    return itr->second;
  }
  return OfficeOpenExtensions::kOther;
}

void LogOneDriveOpenErrorUmaAfterFallback(
    ash::office_fallback::FallbackReason fallback_reason,
    ash::cloud_upload::OfficeTaskResult task_result,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics) {
  switch (fallback_reason) {
    case ash::office_fallback::FallbackReason::kOffline:
      cloud_open_metrics->LogOneDriveOpenError(
          ash::cloud_upload::OfficeOneDriveOpenErrors::kOffline);
      break;
    case ash::office_fallback::FallbackReason::kDriveDisabled:
    case ash::office_fallback::FallbackReason::kNoDriveService:
    case ash::office_fallback::FallbackReason::kDriveAuthenticationNotReady:
    case ash::office_fallback::FallbackReason::kDriveFsInterfaceError:
    case ash::office_fallback::FallbackReason::kMeteredConnection:
      NOTREACHED();
      break;
  }
  cloud_open_metrics->LogTaskResult(task_result);
}

void LogGoogleDriveOpenErrorUmaAfterFallback(
    ash::office_fallback::FallbackReason fallback_reason,
    ash::cloud_upload::OfficeTaskResult task_result,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics) {
  switch (fallback_reason) {
    case ash::office_fallback::FallbackReason::kOffline:
      cloud_open_metrics->LogGoogleDriveOpenError(
          ash::cloud_upload::OfficeDriveOpenErrors::kOffline);
      break;
    case ash::office_fallback::FallbackReason::kDriveDisabled:
      cloud_open_metrics->LogGoogleDriveOpenError(
          ash::cloud_upload::OfficeDriveOpenErrors::kDriveDisabled);
      break;
    case ash::office_fallback::FallbackReason::kNoDriveService:
      cloud_open_metrics->LogGoogleDriveOpenError(
          ash::cloud_upload::OfficeDriveOpenErrors::kNoDriveService);
      break;
    case ash::office_fallback::FallbackReason::kDriveAuthenticationNotReady:
      cloud_open_metrics->LogGoogleDriveOpenError(
          ash::cloud_upload::OfficeDriveOpenErrors::
              kDriveAuthenticationNotReady);
      break;
    case ash::office_fallback::FallbackReason::kDriveFsInterfaceError:
      cloud_open_metrics->LogGoogleDriveOpenError(
          ash::cloud_upload::OfficeDriveOpenErrors::kDriveFsInterface);
      break;
    case ash::office_fallback::FallbackReason::kMeteredConnection:
      cloud_open_metrics->LogGoogleDriveOpenError(
          ash::cloud_upload::OfficeDriveOpenErrors::kMeteredConnection);
      break;
  }
  cloud_open_metrics->LogTaskResult(task_result);
}

absl::optional<ash::office_fallback::FallbackReason>
DriveConnectionStatusToFallbackReason(
    drive::util::ConnectionStatus drive_connection_status) {
  switch (drive_connection_status) {
    case drive::util::ConnectionStatus::kNoService:
      return ash::office_fallback::FallbackReason::kNoDriveService;
    case drive::util::ConnectionStatus::kNoNetwork:
      return ash::office_fallback::FallbackReason::kOffline;
    case drive::util::ConnectionStatus::kNotReady:
      return ash::office_fallback::FallbackReason::kDriveAuthenticationNotReady;
    case drive::util::ConnectionStatus::kMetered:
      return ash::office_fallback::FallbackReason::kMeteredConnection;
    case drive::util::ConnectionStatus::kConnected:
      return absl::nullopt;
  }
}

}  // namespace

void RegisterOfficeProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kOfficeFilesAlwaysMoveToDrive, false);
  registry->RegisterBooleanPref(prefs::kOfficeFilesAlwaysMoveToOneDrive, false);
  registry->RegisterBooleanPref(prefs::kOfficeMoveConfirmationShownForDrive,
                                false);
  registry->RegisterBooleanPref(prefs::kOfficeMoveConfirmationShownForOneDrive,
                                false);
  registry->RegisterBooleanPref(
      prefs::kOfficeMoveConfirmationShownForLocalToDrive, false);
  registry->RegisterBooleanPref(
      prefs::kOfficeMoveConfirmationShownForLocalToOneDrive, false);
  registry->RegisterBooleanPref(
      prefs::kOfficeMoveConfirmationShownForCloudToDrive, false);
  registry->RegisterBooleanPref(
      prefs::kOfficeMoveConfirmationShownForCloudToOneDrive, false);
  registry->RegisterTimePref(prefs::kOfficeFileMovedToOneDrive, base::Time());
  registry->RegisterTimePref(prefs::kOfficeFileMovedToGoogleDrive,
                             base::Time());
}

void RecordOfficeOpenExtensionDriveMetric(
    const storage::FileSystemURL& file_url) {
  UMA_HISTOGRAM_ENUMERATION(
      file_manager::file_tasks::kOfficeOpenExtensionDriveMetricName,
      GetOfficeOpenExtension(file_url));
}

void RecordOfficeOpenExtensionOneDriveMetric(
    const storage::FileSystemURL& file_url) {
  UMA_HISTOGRAM_ENUMERATION(
      file_manager::file_tasks::kOfficeOpenExtensionOneDriveMetricName,
      GetOfficeOpenExtension(file_url));
}

bool ExecuteWebDriveOfficeTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_urls,
    gfx::NativeWindow modal_parent,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics) {
  if (!drive::util::IsDriveEnabledForProfile(profile)) {
    return GetUserFallbackChoice(
        profile, task, file_urls, modal_parent,
        ash::office_fallback::FallbackReason::kDriveDisabled,
        std::move(cloud_open_metrics));
  }

  const drive::util::ConnectionStatus drive_connection_status =
      drive::util::GetDriveConnectionStatus(profile);
  const absl::optional<ash::office_fallback::FallbackReason>
      opt_fallback_reason =
          DriveConnectionStatusToFallbackReason(drive_connection_status);
  if (opt_fallback_reason) {
    return GetUserFallbackChoice(profile, task, file_urls, modal_parent,
                                 opt_fallback_reason.value(),
                                 std::move(cloud_open_metrics));
  }

  drive::DriveIntegrationService* integration_service =
      drive::DriveIntegrationServiceFactory::FindForProfile(profile);
  if (!integration_service || !integration_service->IsMounted() ||
      !integration_service->GetDriveFsInterface()) {
    return GetUserFallbackChoice(
        profile, task, file_urls, modal_parent,
        ash::office_fallback::FallbackReason::kDriveFsInterfaceError,
        std::move(cloud_open_metrics));
  }

  return ash::cloud_upload::CloudOpenTask::Execute(
      profile, file_urls, ash::cloud_upload::CloudProvider::kGoogleDrive,
      modal_parent, std::move(cloud_open_metrics));
}

bool ExecuteOpenInOfficeTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_urls,
    gfx::NativeWindow modal_parent,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics) {
  if (content::GetNetworkConnectionTracker()->IsOffline()) {
    return GetUserFallbackChoice(profile, task, file_urls, modal_parent,
                                 ash::office_fallback::FallbackReason::kOffline,
                                 std::move(cloud_open_metrics));
  }

  return ash::cloud_upload::CloudOpenTask::Execute(
      profile, file_urls, ash::cloud_upload::CloudProvider::kOneDrive,
      modal_parent, std::move(cloud_open_metrics));
}

void LaunchQuickOffice(Profile* profile,
                       const std::vector<storage::FileSystemURL>& file_urls) {
  const TaskDescriptor quick_office_task(
      extension_misc::kQuickOfficeComponentExtensionId, TASK_TYPE_FILE_HANDLER,
      kActionIdQuickOffice);

  ExecuteFileTask(
      profile, quick_office_task, file_urls, /* modal_parent */ nullptr,
      base::BindOnce(
          [](extensions::api::file_manager_private::TaskResult result,
             std::string error_message) {
            if (!error_message.empty()) {
              LOG(ERROR) << "Fallback to QuickOffice for opening office file "
                            "with error message: "
                         << error_message << " and result: " << result;
            }
          }));

  return;
}

void OnDialogChoiceReceived(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_urls,
    gfx::NativeWindow modal_parent,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics,
    const std::string& choice,
    ash::office_fallback::FallbackReason fallback_reason) {
  if (choice == ash::office_fallback::kDialogChoiceQuickOffice) {
    if (IsWebDriveOfficeTask(task)) {
      LogGoogleDriveOpenErrorUmaAfterFallback(
          fallback_reason,
          ash::cloud_upload::OfficeTaskResult::kFallbackQuickOffice,
          std::move(cloud_open_metrics));
    } else if (IsOpenInOfficeTask(task)) {
      LogOneDriveOpenErrorUmaAfterFallback(
          fallback_reason,
          ash::cloud_upload::OfficeTaskResult::kFallbackQuickOffice,
          std::move(cloud_open_metrics));
    }
    LaunchQuickOffice(profile, file_urls);
  } else if (choice == ash::office_fallback::kDialogChoiceTryAgain) {
    // When retrying, the original open result is thrown away, so that
    // (likely the same) result codes from repeated retries are not counted.
    // Only the last open result is recorded: when the user either selects
    // QO or cancels.
    if (IsWebDriveOfficeTask(task)) {
      ExecuteWebDriveOfficeTask(profile, task, file_urls, modal_parent,
                                std::move(cloud_open_metrics));
    } else if (IsOpenInOfficeTask(task)) {
      ExecuteOpenInOfficeTask(profile, task, file_urls, modal_parent,
                              std::move(cloud_open_metrics));
    }
  } else if (choice == ash::office_fallback::kDialogChoiceCancel) {
    if (IsWebDriveOfficeTask(task)) {
      LogGoogleDriveOpenErrorUmaAfterFallback(
          fallback_reason,
          ash::cloud_upload::OfficeTaskResult::kCancelledAtFallback,
          std::move(cloud_open_metrics));
    } else if (IsOpenInOfficeTask(task)) {
      LogOneDriveOpenErrorUmaAfterFallback(
          fallback_reason,
          ash::cloud_upload::OfficeTaskResult::kCancelledAtFallback,
          std::move(cloud_open_metrics));
    }
  }
}

bool GetUserFallbackChoice(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_urls,
    gfx::NativeWindow modal_parent,
    ash::office_fallback::FallbackReason fallback_reason,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics) {
  // If QuickOffice is not installed, don't launch dialog.
  if (!IsQuickOfficeInstalled(profile)) {
    LOG(ERROR) << "Cannot fallback to QuickOffice when it is not installed";
    return false;
  }
  // TODO(b/242685536) Add support for multi-file
  // selection so the OfficeFallbackDialog can display multiple file names and
  // `OnDialogChoiceReceived()` can open multiple files.
  std::vector<storage::FileSystemURL> first_url{file_urls.front()};

  ash::office_fallback::DialogChoiceCallback callback =
      base::BindOnce(&OnDialogChoiceReceived, profile, task, first_url,
                     modal_parent, std::move(cloud_open_metrics));

  const std::string parsed_action_id = ParseFilesAppActionId(task.action_id);

  return ash::office_fallback::OfficeFallbackDialog::Show(
      first_url, fallback_reason, parsed_action_id, std::move(callback));
}

bool IsWebDriveOfficeTask(const TaskDescriptor& task) {
  const std::string action_id = ParseFilesAppActionId(task.action_id);
  bool is_web_drive_office_action_id =
      action_id == kActionIdWebDriveOfficeWord ||
      action_id == kActionIdWebDriveOfficeExcel ||
      action_id == kActionIdWebDriveOfficePowerPoint;
  return IsFilesAppId(task.app_id) && is_web_drive_office_action_id;
}

bool IsOpenInOfficeTask(const TaskDescriptor& task) {
  const std::string action_id = ParseFilesAppActionId(task.action_id);
  return IsFilesAppId(task.app_id) && action_id == kActionIdOpenInOffice;
}

bool IsQuickOfficeInstalled(Profile* profile) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  if (!proxy) {
    return false;
  }
  // The AppRegistryCache will contain the QuickOffice extension whether on Ash
  // or Lacros.
  bool installed = false;
  proxy->AppRegistryCache().ForOneApp(
      extension_misc::kQuickOfficeComponentExtensionId,
      [&installed](const apps::AppUpdate& update) {
        installed = apps_util::IsInstalled(update.Readiness());
      });
  return installed;
}

bool IsOfficeFile(const base::FilePath& path) {
  std::vector<std::set<std::string>> groups = {WordGroupExtensions(),
                                               ExcelGroupExtensions(),
                                               PowerPointGroupExtensions()};

  for (const std::set<std::string>& group : groups) {
    for (const std::string& extension : group) {
      if (path.MatchesExtension(extension)) {
        return true;
      }
    }
  }
  return false;
}

std::set<std::string> WordGroupExtensions() {
  static const base::NoDestructor<std::set<std::string>> extensions(
      std::initializer_list<std::string>({".doc", ".docx"}));
  return *extensions;
}

std::set<std::string> WordGroupMimeTypes() {
  static const base::NoDestructor<std::set<std::string>> mime_types(
      std::initializer_list<std::string>(
          {"application/msword",
           "application/"
           "vnd.openxmlformats-officedocument.wordprocessingml.document"}));
  return *mime_types;
}

bool HasExplicitDefaultFileHandler(Profile* profile,
                                   const std::string& extension) {
  std::string lower_extension = base::ToLowerASCII(extension);
  const base::Value::Dict& extension_task_prefs =
      profile->GetPrefs()->GetDict(prefs::kDefaultTasksBySuffix);
  return extension_task_prefs.contains(lower_extension);
}

void SetWordFileHandler(Profile* profile,
                        const TaskDescriptor& task,
                        bool replace_existing) {
  UpdateDefaultTask(profile, task, WordGroupExtensions(), WordGroupMimeTypes(),
                    replace_existing);
}

void SetWordFileHandlerToFilesSWA(Profile* profile,
                                  const std::string& action_id,
                                  bool replace_existing) {
  TaskDescriptor task(kFileManagerSwaAppId, TaskType::TASK_TYPE_WEB_APP,
                      ToSwaActionId(action_id));
  SetWordFileHandler(profile, task, replace_existing);
}

std::set<std::string> ExcelGroupExtensions() {
  static const base::NoDestructor<std::set<std::string>> extensions(
      std::initializer_list<std::string>({".xls", ".xlsm", ".xlsx"}));
  return *extensions;
}

std::set<std::string> ExcelGroupMimeTypes() {
  static const base::NoDestructor<std::set<std::string>> mime_types(
      std::initializer_list<std::string>(
          {"application/vnd.ms-excel",
           "application/vnd.ms-excel.sheet.macroEnabled.12",
           "application/"
           "vnd.openxmlformats-officedocument.spreadsheetml.sheet"}));
  return *mime_types;
}

void SetExcelFileHandler(Profile* profile,
                         const TaskDescriptor& task,
                         bool replace_existing) {
  UpdateDefaultTask(profile, task, ExcelGroupExtensions(),
                    ExcelGroupMimeTypes(), replace_existing);
}

void SetExcelFileHandlerToFilesSWA(Profile* profile,
                                   const std::string& action_id,
                                   bool replace_existing) {
  TaskDescriptor task(kFileManagerSwaAppId, TaskType::TASK_TYPE_WEB_APP,
                      ToSwaActionId(action_id));
  SetExcelFileHandler(profile, task, replace_existing);
}

std::set<std::string> PowerPointGroupExtensions() {
  static const base::NoDestructor<std::set<std::string>> extensions(
      std::initializer_list<std::string>({".ppt", ".pptx"}));
  return *extensions;
}

std::set<std::string> PowerPointGroupMimeTypes() {
  static const base::NoDestructor<std::set<std::string>> mime_types(
      std::initializer_list<std::string>(
          {"application/vnd.ms-powerpoint",
           "application/"
           "vnd.openxmlformats-officedocument.presentationml.presentation"}));
  return *mime_types;
}

void SetPowerPointFileHandler(Profile* profile,
                              const TaskDescriptor& task,
                              bool replace_existing) {
  UpdateDefaultTask(profile, task, PowerPointGroupExtensions(),
                    PowerPointGroupMimeTypes(), replace_existing);
}

void SetPowerPointFileHandlerToFilesSWA(Profile* profile,
                                        const std::string& action_id,
                                        bool replace_existing) {
  TaskDescriptor task(kFileManagerSwaAppId, TaskType::TASK_TYPE_WEB_APP,
                      ToSwaActionId(action_id));
  SetPowerPointFileHandler(profile, task, replace_existing);
}
void SetAlwaysMoveOfficeFilesToDrive(Profile* profile, bool always_move) {
  profile->GetPrefs()->SetBoolean(prefs::kOfficeFilesAlwaysMoveToDrive,
                                  always_move);
}

bool GetAlwaysMoveOfficeFilesToDrive(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(prefs::kOfficeFilesAlwaysMoveToDrive);
}

void SetAlwaysMoveOfficeFilesToOneDrive(Profile* profile, bool always_move) {
  profile->GetPrefs()->SetBoolean(prefs::kOfficeFilesAlwaysMoveToOneDrive,
                                  always_move);
}

bool GetAlwaysMoveOfficeFilesToOneDrive(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      prefs::kOfficeFilesAlwaysMoveToOneDrive);
}

void SetOfficeMoveConfirmationShownForDrive(Profile* profile, bool complete) {
  profile->GetPrefs()->SetBoolean(prefs::kOfficeMoveConfirmationShownForDrive,
                                  complete);
}

bool GetOfficeMoveConfirmationShownForDrive(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      prefs::kOfficeMoveConfirmationShownForDrive);
}

void SetOfficeMoveConfirmationShownForOneDrive(Profile* profile,
                                               bool complete) {
  profile->GetPrefs()->SetBoolean(
      prefs::kOfficeMoveConfirmationShownForOneDrive, complete);
}

bool GetOfficeMoveConfirmationShownForOneDrive(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      prefs::kOfficeMoveConfirmationShownForOneDrive);
}

void SetOfficeMoveConfirmationShownForLocalToDrive(Profile* profile,
                                                   bool shown) {
  profile->GetPrefs()->SetBoolean(
      prefs::kOfficeMoveConfirmationShownForLocalToDrive, shown);
}

bool GetOfficeMoveConfirmationShownForLocalToDrive(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      prefs::kOfficeMoveConfirmationShownForLocalToDrive);
}

void SetOfficeMoveConfirmationShownForLocalToOneDrive(Profile* profile,
                                                      bool shown) {
  profile->GetPrefs()->SetBoolean(
      prefs::kOfficeMoveConfirmationShownForLocalToOneDrive, shown);
}

bool GetOfficeMoveConfirmationShownForLocalToOneDrive(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      prefs::kOfficeMoveConfirmationShownForLocalToOneDrive);
}

void SetOfficeMoveConfirmationShownForCloudToDrive(Profile* profile,
                                                   bool shown) {
  profile->GetPrefs()->SetBoolean(
      prefs::kOfficeMoveConfirmationShownForCloudToDrive, shown);
}

bool GetOfficeMoveConfirmationShownForCloudToDrive(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      prefs::kOfficeMoveConfirmationShownForCloudToDrive);
}

void SetOfficeMoveConfirmationShownForCloudToOneDrive(Profile* profile,
                                                      bool shown) {
  profile->GetPrefs()->SetBoolean(
      prefs::kOfficeMoveConfirmationShownForCloudToOneDrive, shown);
}

bool GetOfficeMoveConfirmationShownForCloudToOneDrive(Profile* profile) {
  return profile->GetPrefs()->GetBoolean(
      prefs::kOfficeMoveConfirmationShownForCloudToOneDrive);
}

void SetOfficeFileMovedToOneDrive(Profile* profile, base::Time moved) {
  profile->GetPrefs()->SetTime(prefs::kOfficeFileMovedToOneDrive, moved);
}

void SetOfficeFileMovedToGoogleDrive(Profile* profile, base::Time moved) {
  profile->GetPrefs()->SetTime(prefs::kOfficeFileMovedToGoogleDrive, moved);
}

void RemoveFilesSWAWordFileHandler(Profile* profile,
                                   const std::string& action_id) {
  TaskDescriptor task(kFileManagerSwaAppId, TaskType::TASK_TYPE_WEB_APP,
                      ToSwaActionId(action_id));
  RemoveDefaultTask(profile, task, WordGroupExtensions(), WordGroupMimeTypes());
}

void RemoveFilesSWAExcelFileHandler(Profile* profile,
                                    const std::string& action_id) {
  TaskDescriptor task(kFileManagerSwaAppId, TaskType::TASK_TYPE_WEB_APP,
                      ToSwaActionId(action_id));
  RemoveDefaultTask(profile, task, ExcelGroupExtensions(),
                    ExcelGroupMimeTypes());
}

void RemoveFilesSWAPowerPointFileHandler(Profile* profile,
                                         const std::string& action_id) {
  TaskDescriptor task(kFileManagerSwaAppId, TaskType::TASK_TYPE_WEB_APP,
                      ToSwaActionId(action_id));
  RemoveDefaultTask(profile, task, PowerPointGroupExtensions(),
                    PowerPointGroupMimeTypes());
}

}  // namespace file_manager::file_tasks
