// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_OFFICE_TASKS_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_OFFICE_TASKS_H_

#include <map>

#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_system_provider/provided_file_system_interface.h"

namespace file_manager {

// Keeps track of open and upload tasks to ODFS and Google Drive.
class OfficeTasks {
 public:
  OfficeTasks();
  ~OfficeTasks();

  // Keeps track of active `CloudOpenTask`s for a file.
  std::set<base::FilePath> cloud_open_tasks;

  // Keeps track of IO tasks interacting with ODFS.
  std::map<io_task::IOTaskId,
           std::unique_ptr<ash::file_system_provider::ScopedUserInteraction>>
      odfs_interactions;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_OFFICE_TASKS_H_
