// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides utility functions for "file tasks".
//
// WHAT ARE FILE TASKS?
//
// File tasks are actions that can be performed over the currently selected
// files from the Files app. A task can be one of:
//
// 1) A Chrome Extension or App, registered via "file_handlers" or
//    "file_browser_handlers" in manifest.json (ex. Text.app). This information
//    comes from FileBrowserHandler::GetHandlers()
//
// See also: https://developer.chrome.com/extensions/manifest.html#file_handlers
// https://developer.chrome.com/extensions/fileBrowserHandler.html
//
// 2) Built-in handlers provided by the Files app. The Files app provides lots
//    of file_browser_handlers, such as "play", "mount-archive". These built-in
//    handlers are often handled specially inside the Files app. This
//    information also comes from FileBrowserHandler::GetHandlers().
//
// See also: ui/file_manager/file_manager/manifest.json
//
// For example, if the user selects a JPEG file, the Files app will receive file
// tasks represented as a JSON object via
// chrome.fileManagerPrivate.getFileTasks() API, which look like:
//
// [
//   {
//     "iconUrl":
//       "chrome://extension-icon/hhaomjibdihmijegdhdafkllkbggdgoj/16/1",
//     "isDefault": true,
//     "descriptor": {
//       appId: "hhaomjibdihmijegdhdafkllkbggdgoj",
//       taskType: "file",
//       actionId: "gallery"
//     }
//     "title": "__MSG_OPEN_ACTION__"
//   }
// ]
//
// The file task is a built-in handler from the Files app.
//
// WHAT ARE TASK IDS?
//
// "TaskId" is a string of the format "appId|taskType|actionId". We used to
// store these three fields together in a string so we could easily store this
// data in user preferences. We are removing taskId wherever possible in favour
// of the TaskDescriptor struct, which contains the same information but in a
// more typical struct format. TaskId will remain in some parts of the code
// where we need to serialize TaskDescriptors, like for UMA.
//
// What are the three types of information encoded here?
//
// The "TaskId" format encoding is as follows:
//
//     <app-id>|<task-type>|<action-id>
//
// <app-id> is a Chrome Extension/App ID.
//
// <task-type> is either of
// - "file" - File browser handler - app/extension declaring
//            "file_browser_handlers" in manifest.
// - "app" - File handler - app declaring "file_handlers" in manifest.json.
// - "arc" - ARC App
// - "crostini" - Crostini App
//
// <action-id> is an ID string used for identifying actions provided from a
// single Chrome Extension/App. In other words, a single Chrome/Extension can
// provide multiple file handlers hence each of them needs to have a unique
// action ID. For Crostini apps, <action-id> is always "open-with".
//
// HOW ARE TASKS EXECUTED?
//
// chrome.fileManagerPrivate.executeTask() is used to open a file with a handler
// (Chrome Extension/App), and to open files directly in the browser without any
// handler, e.g. PDF.
//
// Files app also has "internal tasks" which we can split into three categories:
//  1. Tasks that open in the browser. The JS-side calls executeTask(), and we
//     spawn a new browser tab here on the C++ side. e.g. "view-in-browser",
//     "view-pdf" and "open-hosted-*".
//  2. Tasks that are handled internally by Files app JS. e.g. "mount-archive",
//     "install-linux-package" and "import-crostini-image".
//  3. Tasks where the browser process opens Files app to a folder or file, e.g.
//     "open" and "select", through file_manager::util::OpenItem().
//
// See also: ui/file_manager/file_manager/foreground/js/file_tasks.js
//

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/functional/callback_forward.h"
#include "chrome/browser/ui/webui/ash/office_fallback/office_fallback_dialog.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "url/gurl.h"

using storage::FileSystemURL;

class PrefService;
class Profile;

namespace extensions {
struct EntryInfo;
}

namespace storage {
class FileSystemURL;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace file_manager::file_tasks {

extern const char kActionIdView[];
extern const char kActionIdSend[];
extern const char kActionIdSendMultiple[];
extern const char kActionIdWebDriveOfficeWord[];
extern const char kActionIdWebDriveOfficeExcel[];
extern const char kActionIdWebDriveOfficePowerPoint[];
extern const char kActionIdOpenInOffice[];
extern const char kActionIdOpenWeb[];

extern const char kODFSExtensionId[];

// Task types as explained in the comment above. Search for <task-type>.
enum TaskType {
  TASK_TYPE_UNKNOWN = 0,  // Used only for handling errors.
  TASK_TYPE_FILE_BROWSER_HANDLER,
  TASK_TYPE_FILE_HANDLER,
  DEPRECATED_TASK_TYPE_DRIVE_APP,
  TASK_TYPE_ARC_APP,
  TASK_TYPE_CROSTINI_APP,
  TASK_TYPE_WEB_APP,
  TASK_TYPE_PLUGIN_VM_APP,
  // The enum values must be kept in sync with FileManagerTaskType in
  // tools/metrics/histograms/enums.xml. Since enums for histograms are
  // append-only (for keeping the number consistent across versions), new values
  // for this enum also has to be always appended at the end (i.e., here).
  NUM_TASK_TYPE,
};

TaskType StringToTaskType(const std::string& str);
std::string TaskTypeToString(TaskType task_type);

constexpr char kDriveErrorMetricName[] = "FileBrowser.OfficeFiles.Errors.Drive";
constexpr char kDriveTaskResultMetricName[] =
    "FileBrowser.OfficeFiles.TaskResult.Drive";

// List of UMA enum value for Web Drive Office task results. The enum values
// must be kept in sync with OfficeTaskResult in
// tools/metrics/histograms/enums.xml.
enum class OfficeTaskResult {
  FALLBACK_QUICKOFFICE = 0,
  FALLBACK_OTHER = 1,
  OPENED = 2,
  MOVED = 3,
  CANCELLED = 4,
  FAILED = 5,
  kMaxValue = FAILED,
};

// List of UMA enum values for Office File Handler task results for Drive. The
// enum values must be kept in sync with OfficeDriveErrors in
// tools/metrics/histograms/enums.xml.
enum class OfficeDriveErrors {
  OFFLINE = 0,
  DRIVEFS_INTERFACE = 1,
  TIMEOUT = 2,
  NO_METADATA = 3,
  INVALID_ALTERNATE_URL = 4,
  DRIVE_ALTERNATE_URL = 5,
  UNEXPECTED_ALTERNATE_URL = 6,
  kMaxValue = UNEXPECTED_ALTERNATE_URL,
};

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

// Describes a task.
// See the comment above for <app-id>, <task-type>, and <action-id>.
struct TaskDescriptor {
  TaskDescriptor(const std::string& in_app_id,
                 TaskType in_task_type,
                 const std::string& in_action_id)
      : app_id(in_app_id), task_type(in_task_type), action_id(in_action_id) {
    // For web apps, the action_id must be a full valid URL if it exists.
    DCHECK(task_type != TASK_TYPE_WEB_APP || action_id.empty() ||
           GURL(action_id).is_valid());
  }
  TaskDescriptor() = default;

  bool operator<(const TaskDescriptor& other) const;
  bool operator==(const TaskDescriptor& other) const;

  std::string app_id;
  TaskType task_type;
  std::string action_id;
};

// Describes a task with extra information such as icon URL.
struct FullTaskDescriptor {
  FullTaskDescriptor(const TaskDescriptor& task_descriptor,
                     const std::string& task_title,
                     const GURL& icon_url,
                     bool is_default,
                     bool is_generic_file_handler,
                     bool is_file_extension_match,
                     bool is_dlp_blocked = false);

  FullTaskDescriptor(const FullTaskDescriptor& other);
  FullTaskDescriptor& operator=(const FullTaskDescriptor& other);

  // Unique ID for the task.
  TaskDescriptor task_descriptor;
  // The user-visible title/name of the app/extension/thing to be launched.
  std::string task_title;
  // The icon URL for the task (ex. app icon)
  GURL icon_url;
  // The default task is stored in user preferences and will be used when the
  // user doesn't explicitly pick another e.g. double click.
  bool is_default;
  // True if this task is from generic file handler. Generic file handler is a
  // file handler which handles any type of files (e.g. extensions: ["*"],
  // types: ["*/*"]). Partial wild card (e.g. types: ["image/*"]) is not
  // generic file handler.
  bool is_generic_file_handler;
  // True if this task is from a file extension only. e.g. an extension/app
  // that declares no MIME types in its manifest, but matches with the
  // file_handlers "extensions" instead.
  bool is_file_extension_match;
  // True if this task is blocked by Data Leak Prevention (DLP).
  bool is_dlp_blocked;
};

// Describes how admin policy affects the default task in a ResultingTasks.
enum class PolicyDefaultHandlerStatus {
  // Indicates that the default task was selected according to the policy
  // settings.
  kDefaultHandlerAssignedByPolicy,

  // Indicates that no default task was set due to some assignment conflicts.
  // Possible reasons are:
  //  * The user is trying to open multiple files which have different policy
  //  default handlers;
  //  * The admin-specified handler was not found in the list of tasks.
  kIncorrectAssignment
};

// Represents a set of tasks capable of handling file entries.
struct ResultingTasks {
  ResultingTasks();
  ~ResultingTasks();

  std::vector<FullTaskDescriptor> tasks;
  absl::optional<PolicyDefaultHandlerStatus> policy_default_handler_status;
};

// Registers profile prefs related to file_manager.
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable*);

// Update the default file handler for the given sets of suffixes and MIME
// types.
void UpdateDefaultTask(Profile* profile,
                       const TaskDescriptor& task_descriptor,
                       const std::set<std::string>& suffixes,
                       const std::set<std::string>& mime_types);

// Returns the default task for the given |mime_type|/|suffix| combination in
// |task_out|. If it finds a MIME type match, then it prefers that over a suffix
// match. If a default can't be found, then it returns false.
bool GetDefaultTaskFromPrefs(const PrefService& pref_service,
                             const std::string& mime_type,
                             const std::string& suffix,
                             TaskDescriptor* task_out);

// Generates task id for the task specified by |app_id|, |task_type| and
// |action_id|.
//
// |app_id| is the Chrome Extension/App ID.
// |action_id| is a free-form string ID for the action.
std::string MakeTaskID(const std::string& app_id,
                       TaskType task_type,
                       const std::string& action_id);

// Converts |task_descriptor| to a task ID.
std::string TaskDescriptorToId(const TaskDescriptor& task_descriptor);

// Parses the task ID and extracts app ID, task type, and action ID into
// |task|. On failure, returns false, and the contents of |task| are
// undefined.
//
// See also the comment at the beginning of the file for details for how
// "task_id" looks like.
bool ParseTaskID(const std::string& task_id, TaskDescriptor* task);

// The callback is used for ExecuteFileTask().
typedef base::OnceCallback<void(
    extensions::api::file_manager_private::TaskResult result,
    std::string error_message)>
    FileTaskFinishedCallback;

// Executes file handler task for each element of |file_urls|.
// Returns |false| if the execution cannot be initiated. Otherwise returns
// |true| and then eventually calls |done| when all the files have been handled.
// |done| can be a null callback.
//
// Parameters:
// profile    - The profile used for making this function call.
// task       - See the comment at TaskDescriptor struct.
// file_urls  - URLs of the target files.
// done       - The callback which will be called on completion.
//              The callback won't be called if the function returns
//              false.
bool ExecuteFileTask(Profile* profile,
                     const TaskDescriptor& task,
                     const std::vector<storage::FileSystemURL>& file_urls,
                     FileTaskFinishedCallback done);

// Executes QuickOffice file handler for each element of |file_urls|.
void LaunchQuickOffice(Profile* profile,
                       const std::vector<storage::FileSystemURL>& file_urls);

// Executes appropriate task to open the selected `file_urls`.
// If user's `choice` is `kDialogChoiceQuickOffice`, launch QuickOffice.
// If user's `choice` is `kDialogChoiceTryAgain`, execute the `task`.
// If user's `choice` is `kDialogChoiceCancel`, do nothing.
void OnDialogChoiceReceived(Profile* profile,
                            const TaskDescriptor& task,
                            const std::vector<FileSystemURL>& file_urls,
                            const std::string& choice);

// Shows a new dialog for users to choose what to do next. Returns True
// if a new dialog has been effectively created.
bool GetUserFallbackChoice(Profile* profile,
                           const TaskDescriptor& task,
                           const std::vector<FileSystemURL>& file_urls,
                           ash::office_fallback::FallbackReason failure_reason);

// Callback function type for FindAllTypesOfTasks.
typedef base::OnceCallback<void(
    std::unique_ptr<ResultingTasks> resulting_tasks)>
    FindTasksCallback;

// Finds all types (file handlers, file browser handlers) of
// tasks.
//
// If |entries| contains a Google document, only the internal tasks of the
// Files app (i.e., tasks having the app ID of the Files app) are listed.
// This is to avoid listing normal file handler and file browser handler tasks,
// which can handle only normal files. If passed, |dlp_source_urls| should have
// the same length as |entries| and each element should represent the URL from
// which the corresponding entry was downloaded from, and are used to check DLP
// restrictions on the |entries|.
void FindAllTypesOfTasks(Profile* profile,
                         const std::vector<extensions::EntryInfo>& entries,
                         const std::vector<GURL>& file_urls,
                         const std::vector<std::string>& dlp_source_urls,
                         FindTasksCallback callback);

// Chooses the default task in |resulting_tasks| and sets it as default, if the
// default task is found (i.e. the default task may not exist in
// |resulting_tasks|). No tasks should be set as default before calling this
// function.
void ChooseAndSetDefaultTask(Profile* profile,
                             const std::vector<extensions::EntryInfo>& entries,
                             ResultingTasks* resulting_tasks);

bool IsWebDriveOfficeTask(const TaskDescriptor& task);

bool IsOpenInOfficeTask(const TaskDescriptor& task);

bool IsExtensionInstalled(Profile* profile, const std::string& extension_id);

// Returns whether |path| is an HTML file according to its extension.
bool IsHtmlFile(const base::FilePath& path);

// Returns whether |path| is a MS Office file according to its extension.
bool IsOfficeFile(const base::FilePath& path);

// Updates the default task for each of the office file types.
void SetWordFileHandler(Profile* profile, TaskDescriptor& task);
void SetExcelFileHandler(Profile* profile, TaskDescriptor& task);
void SetPowerPointFileHandler(Profile* profile, TaskDescriptor& task);

// TODO(petermarshall): Move these to a new file office_file_tasks.cc/h
// Updates the default task for each of the office file types to a Files
// SWA with |action_id|. |action_id| must be a valid action registered with the
// Files app SWA.
void SetWordFileHandlerToFilesSWA(Profile* profile,
                                  const std::string& action_id);
void SetExcelFileHandlerToFilesSWA(Profile* profile,
                                   const std::string& action_id);
void SetPowerPointFileHandlerToFilesSWA(Profile* profile,
                                        const std::string& action_id);

// TODO(petermarshall): Move these to a new file office_file_tasks.cc/h
// Sets the user preference storing whether the setup flow for office files has
// ever been completed.
void SetOfficeSetupComplete(Profile* profile, bool complete = true);
// Whether or not the setup flow for office files has ever been completed.
bool OfficeSetupComplete(Profile* profile);

// Sets the user preference storing whether we should always move office files
// without first asking the user.
void SetAlwaysMoveOfficeFiles(Profile* profile, bool complete = true);
// Whether we should always move office files without first asking the user.
bool AlwaysMoveOfficeFiles(Profile* profile);

// Sets the preference `office.file_moved_one_drive`.
void SetOfficeFileMovedToOneDrive(Profile* profile, base::Time moved);
// Sets the preference `office.file_moved_google_drive`.
void SetOfficeFileMovedToGoogleDrive(Profile* profile, base::Time moved);

}  // namespace file_manager::file_tasks

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_TASKS_H_
