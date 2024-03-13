// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_OFFICE_FILE_TASKS_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_OFFICE_FILE_TASKS_H_

#include <set>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_util.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_dialog.h"

class Profile;

namespace ash::cloud_upload {
class CloudOpenMetrics;
}

namespace storage {
class FileSystemURL;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace file_manager::file_tasks {

struct TaskDescriptor;

constexpr char kActionIdQuickOffice[] = "/views/app.html";
constexpr char kActionIdWebDriveOfficeWord[] = "open-web-drive-office-word";
constexpr char kActionIdWebDriveOfficeExcel[] = "open-web-drive-office-excel";
constexpr char kActionIdWebDriveOfficePowerPoint[] =
    "open-web-drive-office-powerpoint";
constexpr char kActionIdOpenInOffice[] = "open-in-office";

// UMA metric name that tracks the result of using a MS Office file outside
// of Drive.
constexpr char kUseOutsideDriveMetricName[] =
    "FileBrowser.OfficeFiles.UseOutsideDrive";

// List of UMA enum values for file system operations that let a user use a
// MS Office file outside of Drive. The enum values must be kept in sync with
// OfficeFilesUseOutsideDriveHook in tools/metrics/histograms/enums.xml.
enum class OfficeFilesUseOutsideDriveHook {
  FILE_PICKER_SELECTION = 0,
  COPY = 1,
  MOVE = 2,
  ZIP = 3,
  OPEN_FROM_FILES_APP = 4,
  kMaxValue = OPEN_FROM_FILES_APP,
};

// UMA metric name that tracks the extension of Office files that are being
// opened with Drive web.
constexpr char kOfficeOpenExtensionDriveMetricName[] =
    "FileBrowser.OfficeFiles.Open.FileType.GoogleDrive";

// UMA metric name that tracks the extension of Office files that are being
// opened with MS365.
constexpr char kOfficeOpenExtensionOneDriveMetricName[] =
    "FileBrowser.OfficeFiles.Open.FileType.OneDrive";

// List of file extensions that are used when opening a file with the
// "open-in-office" task. The enum values must be kept in sync with
// OfficeOpenExtensions in tools/metrics/histograms/enums.xml.
enum class OfficeOpenExtensions {
  kOther,
  kDoc,
  kDocm,
  kDocx,
  kDotm,
  kDotx,
  kOdp,
  kOds,
  kOdt,
  kPot,
  kPotm,
  kPotx,
  kPpam,
  kPps,
  kPpsm,
  kPpsx,
  kPpt,
  kPptm,
  kPptx,
  kXls,
  kXlsb,
  kXlsm,
  kXlsx,
  kMaxValue = kXlsx,
};

// Registers profile prefs related to file_manager.
void RegisterOfficeProfilePrefs(user_prefs::PrefRegistrySyncable*);

void RecordOfficeOpenExtensionDriveMetric(
    const storage::FileSystemURL& file_url);
void RecordOfficeOpenExtensionOneDriveMetric(
    const storage::FileSystemURL& file_url);

// Open files with WebDrive.
bool ExecuteWebDriveOfficeTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_urls,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics);

// Open files with Office365.
bool ExecuteOpenInOfficeTask(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_urls,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics);

// Executes QuickOffice file handler for each element of |file_urls|.
void LaunchQuickOffice(Profile* profile,
                       const std::vector<storage::FileSystemURL>& file_urls);

void LogOneDriveMetricsAfterFallback(
    ash::office_fallback::FallbackReason fallback_reason,
    ash::cloud_upload::OfficeTaskResult task_result,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics);

void LogGoogleDriveMetricsAfterFallback(
    ash::office_fallback::FallbackReason fallback_reason,
    ash::cloud_upload::OfficeTaskResult task_result,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics);

// Executes appropriate task to open the selected `file_urls`.
// If user's `choice` is `kDialogChoiceQuickOffice`, launch QuickOffice.
// If user's `choice` is `kDialogChoiceTryAgain`, execute the `task`.
// If user's `choice` is `kDialogChoiceCancel`, do nothing.
void OnDialogChoiceReceived(
    Profile* profile,
    const TaskDescriptor& task,
    const std::vector<storage::FileSystemURL>& file_urls,
    ash::office_fallback::FallbackReason fallback_reason,
    std::unique_ptr<ash::cloud_upload::CloudOpenMetrics> cloud_open_metrics,
    std::optional<const std::string> choice);

// Shows a new dialog for users to choose what to do next. Returns True
// if a new dialog has been effectively created.
bool GetUserFallbackChoice(Profile* profile,
                           const TaskDescriptor& task,
                           const std::vector<storage::FileSystemURL>& file_urls,
                           ash::office_fallback::FallbackReason failure_reason,
                           ash::office_fallback::DialogChoiceCallback callback);

bool IsWebDriveOfficeTask(const TaskDescriptor& task);

bool IsOpenInOfficeTask(const TaskDescriptor& task);

bool IsQuickOfficeInstalled(Profile* profile);

// Returns whether |path| is a MS Office file according to its extension.
bool IsOfficeFile(const base::FilePath& path);

// Returns whether |mime_type| is a MS Office file mimetype.
bool IsOfficeFileMimeType(const std::string& mime_type);

// Returns the group of extensions we consider to be 'Word', 'Excel' or
// 'PowerPoint' files for the purpose of setting preferences. The extensions
// contain the '.' character at the start.
std::set<std::string> WordGroupExtensions();
std::set<std::string> ExcelGroupExtensions();
std::set<std::string> PowerPointGroupExtensions();

// The same as above but MIME types.
std::set<std::string> WordGroupMimeTypes();
std::set<std::string> ExcelGroupMimeTypes();
std::set<std::string> PowerPointGroupMimeTypes();

// Updates the default task for each of the office file types.
void SetWordFileHandler(Profile* profile,
                        const TaskDescriptor& task,
                        bool replace_existing = true);
void SetExcelFileHandler(Profile* profile,
                         const TaskDescriptor& task,
                         bool replace_existing = true);
void SetPowerPointFileHandler(Profile* profile,
                              const TaskDescriptor& task,
                              bool replace_existing = true);

// Whether we have an explicit user preference stored for the file handler for
// this extension. |extension| should contain the leading '.'.
bool HasExplicitDefaultFileHandler(Profile* profile,
                                   const std::string& extension);

// Updates the default task for each of the office file types to a Files
// SWA with |action_id|. |action_id| must be a valid action registered with the
// Files app SWA.
void SetWordFileHandlerToFilesSWA(Profile* profile,
                                  const std::string& action_id,
                                  bool replace_existing = true);
void SetExcelFileHandlerToFilesSWA(Profile* profile,
                                   const std::string& action_id,
                                   bool replace_existing = true);
void SetPowerPointFileHandlerToFilesSWA(Profile* profile,
                                        const std::string& action_id,
                                        bool replace_existing = true);

// Removes the specified default task for |action_id| for each of the office
// file types.
void RemoveFilesSWAWordFileHandler(Profile* profile,
                                   const std::string& action_id);
void RemoveFilesSWAExcelFileHandler(Profile* profile,
                                    const std::string& action_id);
void RemoveFilesSWAPowerPointFileHandler(Profile* profile,
                                         const std::string& action_id);

// Sets the user preference storing whether we should always move office files
// to Google Drive without first asking the user.
void SetAlwaysMoveOfficeFilesToDrive(Profile* profile, bool complete = true);
// Whether we should always move office files to Google Drive without first
// asking the user.
bool GetAlwaysMoveOfficeFilesToDrive(Profile* profile);

// Sets the user preference storing whether we should always move office files
// to OneDrive without first asking the user.
void SetAlwaysMoveOfficeFilesToOneDrive(Profile* profile, bool complete = true);
// Whether we should always move office files to OneDrive without first asking
// the user.
bool GetAlwaysMoveOfficeFilesToOneDrive(Profile* profile);

// Sets the user preference storing whether the move confirmation dialog has
// been shown before for moving files to Drive.
void SetOfficeMoveConfirmationShownForDrive(Profile* profile, bool complete);
// Whether the move confirmation dialog has been shown before for moving files
// to Drive.
bool GetOfficeMoveConfirmationShownForDrive(Profile* profile);

// Sets the user preference storing whether the move confirmation dialog has
// been shown before for moving files to OneDrive.
void SetOfficeMoveConfirmationShownForOneDrive(Profile* profile, bool complete);
// Whether the move confirmation dialog has been shown before for moving files
// to OneDrive.
bool GetOfficeMoveConfirmationShownForOneDrive(Profile* profile);

// Sets the user preference storing whether the move confirmation dialog has
// been shown before for uploading files from a local source to Drive.
void SetOfficeMoveConfirmationShownForLocalToDrive(Profile* profile,
                                                   bool shown);
// Whether the move confirmation dialog has been shown before for uploading
// files from a local source to Drive.
bool GetOfficeMoveConfirmationShownForLocalToDrive(Profile* profile);

// Sets the user preference storing whether the move confirmation dialog has
// been shown before for uploading files from a local source to OneDrive.
void SetOfficeMoveConfirmationShownForLocalToOneDrive(Profile* profile,
                                                      bool shown);
// Whether the move confirmation dialog has been shown before for uploading
// files from a local source to OneDrive.
bool GetOfficeMoveConfirmationShownForLocalToOneDrive(Profile* profile);

// Sets the user preference storing whether the move confirmation dialog has
// been shown before for uploading files from a cloud source to Drive.
void SetOfficeMoveConfirmationShownForCloudToDrive(Profile* profile,
                                                   bool shown);
// Whether the move confirmation dialog has been shown before for uploading
// files from a cloud source to Drive.
bool GetOfficeMoveConfirmationShownForCloudToDrive(Profile* profile);

// Sets the user preference storing whether the move confirmation dialog has
// been shown before for uploading files from a cloud source to OneDrive.
void SetOfficeMoveConfirmationShownForCloudToOneDrive(Profile* profile,
                                                      bool shown);
// Whether the move confirmation dialog has been shown before for uploading
// files from a cloud source to OneDrive.
bool GetOfficeMoveConfirmationShownForCloudToOneDrive(Profile* profile);

// Sets the preference `office.file_moved_one_drive`.
void SetOfficeFileMovedToOneDrive(Profile* profile, base::Time moved);
// Sets the preference `office.file_moved_google_drive`.
void SetOfficeFileMovedToGoogleDrive(Profile* profile, base::Time moved);

}  // namespace file_manager::file_tasks

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_OFFICE_FILE_TASKS_H_
