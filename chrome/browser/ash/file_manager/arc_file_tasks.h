// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_ARC_FILE_TASKS_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_ARC_FILE_TASKS_H_

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/ash/file_manager/file_tasks.h"

class Profile;

namespace extensions {
struct EntryInfo;
}

namespace storage {
class FileSystemURL;
}

namespace file_manager::file_tasks {

// Finds the ARC tasks that can handle |entries|, appends them to
// |resulting_tasks|, and calls back to |callback|.
void FindArcTasks(Profile* profile,
                  const std::vector<extensions::EntryInfo>& entries,
                  const std::vector<GURL>& file_urls,
                  std::unique_ptr<ResultingTasks> resulting_tasks,
                  FindTasksCallback callback);

// Executes the specified task by ARC.
void ExecuteArcTask(Profile* profile,
                    const TaskDescriptor& task,
                    const std::vector<storage::FileSystemURL>& file_system_urls,
                    const std::vector<std::string>& mime_types,
                    FileTaskFinishedCallback done);

}  // namespace file_manager::file_tasks

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_ARC_FILE_TASKS_H_
