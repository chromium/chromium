// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_TASKS_INSTALL_ISOLATED_WEB_APP_VIRTUAL_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_TASKS_INSTALL_ISOLATED_WEB_APP_VIRTUAL_TASK_H_

#include <string>
#include <vector>

#include "chrome/browser/ash/file_manager/virtual_file_tasks.h"

class GURL;
class Profile;

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace file_manager::file_tasks {

struct TaskDescriptor;

// A task to launch the Isolated Web App installation UI when a .swbn file
// is opened.
class InstallIsolatedWebAppVirtualTask : public VirtualTask {
 public:
  InstallIsolatedWebAppVirtualTask();

  bool IsEnabled(Profile* profile) const override;

  std::string id() const override;

  std::string title() const override;

  GURL icon_url() const override;

  bool IsDlpBlocked(
      const std::vector<std::string>& dlp_source_urls) const override;

  bool Execute(
      Profile* profile,
      const TaskDescriptor& task,
      const std::vector<storage::FileSystemURL>& file_urls) const override;
};

}  // namespace file_manager::file_tasks

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_TASKS_INSTALL_ISOLATED_WEB_APP_VIRTUAL_TASK_H_
