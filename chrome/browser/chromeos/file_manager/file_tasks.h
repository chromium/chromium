// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file provides utility functions for "file tasks".
//
// WHAT ARE FILE TASKS?
//
// File tasks are representation of actions that can be performed over the
// currently selected files from the Files app. A task can be either of:
//
// 1) Chrome extension or app, registered via "file_handlers" or
// "file_browser_handlers" in manifest.json (ex. Text.app). This information
// comes from FileBrowserHandler::GetHandlers()
//
// See also:
// https://developer.chrome.com/extensions/manifest.html#file_handlers
// https://developer.chrome.com/extensions/fileBrowserHandler.html
//
// 2) Built-in handlers provided from the Files app. The Files app provides
// lots of file_browser_handlers, such as "play", "mount-archive".  These
// built-in handlers are often handled in special manners inside the Files
// app. This information also comes from FileBrowserHandler::GetHandlers().
//
// See also:
// ui/file_manager/file_manager/manifest.json
//
// For example, if the user is now selecting a JPEG file, the Files app will
// receive file tasks represented as a JSON object via
// chrome.fileManagerPrivate.getFileTasks() API, which look like:
//
// [
//   {
//     "iconUrl":
//       "chrome://extension-icon/hhaomjibdihmijegdhdafkllkbggdgoj/16/1",
//     "isDefault": true,
//     "taskId": "hhaomjibdihmijegdhdafkllkbggdgoj|file|gallery",
//     "title": "__MSG_OPEN_ACTION__"
//   }
// ]
//
// The file task is a built-in handler from the Files app.
//
// WHAT ARE TASK IDS?
//
// You may have noticed that "taskId" fields in the above example look
// awkward. Apparently "taskId" encodes three types of information delimited
// by "|". This is a weird format for something called as an ID.
//
// 1) Why are the three types information encoded in this way?
//
// It's just a historical reason. The reason is that a simple string can be
// easily stored in user's preferences. We should stop doing this, by storing
// this information in chrome.storage instead. crbug.com/267359.
//
// 2) OK, then what are the three types of information encoded here?
//
// The task ID encodes the following structure:
//
//     <app-id>|<task-type>|<task-action-id>
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
// <task-action-id> is an ID string used for identifying actions provided
// from a single Chrome Extension/App. In other words, a single
// Chrome/Extension can provide multiple file handlers hence each of them
// needs to have a unique action ID. For Crostini apps, <task-action-id> is
// always "open-with".
//
// HOW TASKS ARE EXECUTED?
//
// chrome.fileManagerPrivate.viewFiles() is used to open a file in a browser,
// without any handler. Browser will take care of handling the file (ex. PDF).
//
// chrome.fileManagerPrivate.executeTasks() is used to open a file with a
// handler (Chrome Extension/App).
//
// Some built-in handlers such as "play" are handled internally in the Files
// app. "mount-archive" is handled very differently. The task execution
// business should be simplified: crbug.com/267313
//
// See also:
// ui/file_manager/file_manager/foreground/js/file_tasks.js
//

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_TASKS_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_TASKS_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "chrome/common/extensions/api/file_manager_private.h"
#include "extensions/browser/api/file_handlers/app_file_handler_util.h"
#include "url/gurl.h"

class PrefService;
class Profile;

namespace apps {
struct FileHandlerInfo;
}

namespace extensions {
struct EntryInfo;
}

namespace storage {
class FileSystemURL;
}

namespace file_manager {
namespace file_tasks {

// Task types as explained in the comment above. Search for <task-type>.
enum TaskType {
  TASK_TYPE_UNKNOWN = 0,  // Used only for handling errors.
  TASK_TYPE_FILE_BROWSER_HANDLER,
  TASK_TYPE_FILE_HANDLER,
  DEPRECATED_TASK_TYPE_DRIVE_APP,
  TASK_TYPE_ARC_APP,
  TASK_TYPE_CROSTINI_APP,
  TASK_TYPE_WEB_APP,
  // The enum values must be kept in sync with FileManagerTaskType in
  // tools/metrics/histograms/enums.xml. Since enums for histograms are
  // append-only (for keeping the number consistent across versions), new values
  // for this enum also has to be always appended at the end (i.e., here).
  NUM_TASK_TYPE,
};

// Describes a task.
// See the comment above for <app-id>, <task-type>, and <action-id>.
struct TaskDescriptor {
  TaskDescriptor(const std::string& in_app_id,
                 TaskType in_task_type,
                 const std::string& in_action_id)
      : app_id(in_app_id),
        task_type(in_task_type),
        action_id(in_action_id) {
  }
  TaskDescriptor() = default;

  std::string app_id;
  TaskType task_type;
  std::string action_id;
};

// Describes a task with extra information such as icon URL.
class FullTaskDescriptor {
 public:
  FullTaskDescriptor(
      const TaskDescriptor& task_descriptor,
      const std::string& task_title,
      const extensions::api::file_manager_private::Verb task_verb,
      const GURL& icon_url,
      bool is_default,
      bool is_generic_file_handler,
      bool is_file_extension_match);

  ~FullTaskDescriptor();

  FullTaskDescriptor(const FullTaskDescriptor& other);
  const TaskDescriptor& task_descriptor() const { return task_descriptor_; }

  // The title of the task.
  const std::string& task_title() const { return task_title_; }
  // The verb of the task.
  extensions::api::file_manager_private::Verb task_verb() const {
    return task_verb_;
  }
  // The icon URL for the task (ex. app icon)
  const GURL& icon_url() const { return icon_url_; }

  // True if this task is set as default.
  bool is_default() const { return is_default_; }
  void set_is_default(bool is_default) { is_default_ = is_default; }

  // True if this task is from generic file handler. Generic file handler is a
  // file handler which handles any type of files (e.g. extensions: ["*"],
  // types: ["*/*"]). Partial wild card (e.g. types: ["image/*"]) is not
  // generic file handler.
  bool is_generic_file_handler() const { return is_generic_file_handler_; }
  void set_is_generic_file_handler(bool is_generic_file_handler) {
    is_generic_file_handler_ = is_generic_file_handler;
  }
  // True if this task is from a file extension only. e.g. an extension/app
  // that declares no MIME types in its manifest, but matches with the
  // file_handlers "extensions" instead.
  bool is_file_extension_match() const { return is_file_extension_match_; }
  void set_is_file_extension_match(bool is_file_extension_match) {
    is_file_extension_match_ = is_file_extension_match;
  }

 private:
  TaskDescriptor task_descriptor_;
  std::string task_title_;
  extensions::api::file_manager_private::Verb task_verb_;
  GURL icon_url_;
  bool is_default_;
  bool is_generic_file_handler_;
  bool is_file_extension_match_;
};

// Update the default file handler for the given sets of suffixes and MIME
// types.
void UpdateDefaultTask(PrefService* pref_service,
                       const std::string& task_id,
                       const std::set<std::string>& suffixes,
                       const std::set<std::string>& mime_types);

// Returns the task ID of the default task for the given |mime_type|/|suffix|
// combination. If it finds a MIME type match, then it prefers that over a
// suffix match. If it a default can't be found, then it returns the empty
// string.
std::string GetDefaultTaskIdFromPrefs(const PrefService& pref_service,
                                      const std::string& mime_type,
                                      const std::string& suffix);

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
// source_url - The source URL which originates this function call.
// task       - See the comment at TaskDescriptor struct.
// file_urls  - URLs of the target files.
// done       - The callback which will be called on completion.
//              The callback won't be called if the function returns
//              false.
bool ExecuteFileTask(Profile* profile,
                     const GURL& source_url,
                     const TaskDescriptor& task,
                     const std::vector<storage::FileSystemURL>& file_urls,
                     FileTaskFinishedCallback done);

// Returns true if a file handler is enabled. Some handlers such as
// import-crostini-image can be disabled at runtime by enterprise policy.
bool IsFileHandlerEnabled(Profile* profile,
                          const apps::FileHandlerInfo& file_handler_info);

// Returns true if a file handler matches with entries as good match.
bool IsGoodMatchFileHandler(const apps::FileHandlerInfo& file_handler_info,
                            const std::vector<extensions::EntryInfo>& entries);

// Finds the file handler tasks (apps declaring "file_handlers" in
// manifest.json) that can be used with the given entries, appending them to
// the |result_list|.
void FindFileHandlerTasks(Profile* profile,
                          const std::vector<extensions::EntryInfo>& entries,
                          std::vector<FullTaskDescriptor>* result_list);

// Finds the file browser handler tasks (app/extensions declaring
// "file_browser_handlers" in manifest.json) that can be used with the
// given files, appending them to the |result_list|.
void FindFileBrowserHandlerTasks(
    Profile* profile,
    const std::vector<GURL>& file_urls,
    std::vector<FullTaskDescriptor>* result_list);

// Callback function type for FindAllTypesOfTasks.
typedef base::OnceCallback<void(
    std::unique_ptr<std::vector<FullTaskDescriptor>> result)>
    FindTasksCallback;

// Finds all types (file handlers, file browser handlers) of
// tasks.
//
// If |entries| contains a Google document, only the internal tasks of the
// Files app (i.e., tasks having the app ID of the Files app) are listed.
// This is to avoid listing normal file handler and file browser handler tasks,
// which can handle only normal files.
void FindAllTypesOfTasks(Profile* profile,
                         const std::vector<extensions::EntryInfo>& entries,
                         const std::vector<GURL>& file_urls,
                         FindTasksCallback callback);

// Chooses the default task in |tasks| and sets it as default, if the default
// task is found (i.e. the default task may not exist in |tasks|). No tasks
// should be set as default before calling this function.
void ChooseAndSetDefaultTask(const PrefService& pref_service,
                             const std::vector<extensions::EntryInfo>& entries,
                             std::vector<FullTaskDescriptor>* tasks);

}  // namespace file_tasks
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_FILE_TASKS_H_
