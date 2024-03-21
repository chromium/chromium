// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_FILE_TASKS_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_FILE_TASKS_H_

#include <vector>

#include "chrome/browser/ash/file_manager/file_tasks.h"
#include "extensions/browser/entry_info.h"
#include "url/gurl.h"

class Profile;

namespace file_manager::file_tasks {

// VirtualTasks are tasks that are not implemented using any 'app' file handling
// API, but instead have their implementation in C++. These used to be listed in
// the Files app manifest, but they were never implemented in Files app. We
// still use the Files app ID (kFileManagerSwaAppId) to store these tasks in
// prefs, but this could be migrated in the future. All registered VirtualTasks
// are listed in |GetVirtualTasks()|.
class VirtualTask {
 public:
  VirtualTask();
  virtual ~VirtualTask();

  // Disallow copy and assign.
  VirtualTask(const VirtualTask&) = delete;
  VirtualTask& operator=(const VirtualTask&) = delete;

  // Execute the task and return an indication of whether it completed, or
  // started running, if there are async steps.
  virtual bool Execute(Profile* profile,
                       const TaskDescriptor& task,
                       const std::vector<FileSystemURL>& file_urls) const = 0;
  // Whether this task should be included in |MatchVirtualTasks()|. This can be
  // used to disable tasks based on a flag or other runtime conditions.
  virtual bool IsEnabled(Profile* profile) const = 0;
  // Whether this task should be available to execute on the supplied files, if
  // enabled. |Matches()| can return true even if the task is disabled - in this
  // case the task will not be found by |MatchVirtualTasks()|. Note this has a
  // default implementation which matches against file extensions and mime types
  // in |matcher_mime_types_| and |matcher_file_extensions_|.
  virtual bool Matches(const std::vector<extensions::EntryInfo>& entries,
                       const std::vector<GURL>& file_urls) const;

  // The ID of this task, which is unique across all virtual tasks. Used for
  // storing in preferences, and referring to this task in a TaskDescriptor.
  // These values currently match legacy values from the Files app manifest.
  virtual std::string id() const = 0;
  // The user-visible icon in Files app. This can be overridden in Files app
  // frontend in file_tasks.ts, based on action ID.
  virtual GURL icon_url() const = 0;
  // The user-visible title in Files app - make sure it's translated.
  virtual std::string title() const = 0;
  // Whether the execution of this task should be blocked by DLP (Data Leak
  // Prevention). Files app will show the task as greyed out if it otherwise
  // matches the file URLs and is enabled. |dlp_source_urls| represents the URLs
  // from which the |entries| passed to |Matches()| were downloaded. The URLs
  // are empty strings if not tracked by DLP.
  virtual bool IsDlpBlocked(
      const std::vector<std::string>& dlp_source_urls) const = 0;

 protected:
  std::vector<std::string> matcher_mime_types_;
  // File extensions without the leading ".".
  std::vector<std::string> matcher_file_extensions_;
};

// Appends any virtual tasks that are enabled and match |entries|/|file_urls| to
// |result_list|.
void MatchVirtualTasks(Profile* profile,
                       const std::vector<extensions::EntryInfo>& entries,
                       const std::vector<GURL>& file_urls,
                       const std::vector<std::string>& dlp_source_urls,
                       std::vector<FullTaskDescriptor>* result_list);

// Run |task| by calling |Execute()| on the associated VirtualTask.
bool ExecuteVirtualTask(Profile* profile,
                        const TaskDescriptor& task,
                        const std::vector<FileSystemURL>& file_urls);

// Whether |task| is a virtual task and can be executed using
// |ExecuteVirtualTask()|. Returns true for disabled tasks, too.
bool IsVirtualTask(const TaskDescriptor& task);

// Look up/get the VirtualTask for |task| if it exists, or nullptr.
VirtualTask* FindVirtualTask(const TaskDescriptor& task);

// Tests can insert into this vector and it will be used instead of the real
// tasks if it's non-empty.
std::vector<VirtualTask*>& GetTestVirtualTasks();

}  // namespace file_manager::file_tasks

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_FILE_TASKS_H_
