// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_FILE_MANAGER_WEB_FILE_TASKS_H_
#define CHROME_BROWSER_CHROMEOS_FILE_MANAGER_WEB_FILE_TASKS_H_

#include <vector>

#include "chrome/browser/chromeos/file_manager/file_tasks.h"

class Profile;

namespace extensions {
struct EntryInfo;
}

namespace storage {
class FileSystemURL;
}

namespace file_manager {
namespace file_tasks {

// Finds the web app tasks that can handle |entries|, appends them to
// |result_list|.
void FindWebTasks(Profile* profile,
                  const std::vector<extensions::EntryInfo>& entries,
                  std::vector<FullTaskDescriptor>* result_list);

// Executes the specified web task.
void ExecuteWebTask(Profile* profile,
                    const TaskDescriptor& task,
                    const std::vector<storage::FileSystemURL>& file_system_urls,
                    FileTaskFinishedCallback done);

}  // namespace file_tasks
}  // namespace file_manager

#endif  // CHROME_BROWSER_CHROMEOS_FILE_MANAGER_WEB_FILE_TASKS_H_
