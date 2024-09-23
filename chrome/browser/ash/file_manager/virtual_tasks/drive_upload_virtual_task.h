// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_TASKS_DRIVE_UPLOAD_VIRTUAL_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_TASKS_DRIVE_UPLOAD_VIRTUAL_TASK_H_

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

// A base task to launch the upload-to-cloud workflow for Google
// Docs/Sheets/Slides and Google Drive. Docs, Sheets, and Slides share a base
// Virtual Task because they have the same task implementation. They just have
// different names and icons, and handle separate file types.
class DriveUploadBaseVirtualTask : public VirtualTask {
 public:
  bool IsEnabled(Profile* profile) const override;

  bool IsDlpBlocked(
      const std::vector<std::string>& dlp_source_urls) const override;

  bool Execute(
      Profile* profile,
      const TaskDescriptor& task,
      const std::vector<storage::FileSystemURL>& file_urls) const override;

 protected:
  DriveUploadBaseVirtualTask();
};

class DocsUploadVirtualTask : public DriveUploadBaseVirtualTask {
 public:
  DocsUploadVirtualTask();

  std::string id() const override;
  std::string title() const override;
  GURL icon_url() const override;
};

class SheetsUploadVirtualTask : public DriveUploadBaseVirtualTask {
 public:
  SheetsUploadVirtualTask();

  std::string id() const override;
  std::string title() const override;
  GURL icon_url() const override;
};

class SlidesUploadVirtualTask : public DriveUploadBaseVirtualTask {
 public:
  SlidesUploadVirtualTask();

  std::string id() const override;
  std::string title() const override;
  GURL icon_url() const override;
};

}  // namespace file_manager::file_tasks

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_VIRTUAL_TASKS_DRIVE_UPLOAD_VIRTUAL_TASK_H_
