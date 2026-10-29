// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_FILE_DELETER_H_
#define CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_FILE_DELETER_H_

#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_controller.h"

namespace ash::cloud_upload {

// Helper class to delete file on OneDrive and execute a callback upon
// completion.
class OdfsFileDeleter
    : public file_manager::io_task::IOTaskController::Observer {
 public:
  // Deletes the file at the given `path`. `callback`, if not null, will be
  // executed upon completion with boolean indicating whether the deletion was
  // successful.
  static void Delete(const base::FilePath& path,
                     base::OnceCallback<void(bool delete_successful)> callback);

 private:
  OdfsFileDeleter(const base::FilePath& path,
                  base::OnceCallback<void(bool delete_successful)> callback);
  ~OdfsFileDeleter() override;

  void StartDeletion();
  void FinishDeletion(bool delete_successful);

  // IOTaskController::Observer:
  void OnIOTaskStatus(
      const ::file_manager::io_task::ProgressStatus& status) override;

  ::file_manager::io_task::IOTaskId task_id_;
  const base::FilePath path_;
  raw_ptr<::file_manager::io_task::IOTaskController> io_task_controller_ =
      nullptr;
  base::OnceCallback<void(bool delete_successful)> callback_;
};

}  // namespace ash::cloud_upload

#endif  // CHROME_BROWSER_ASH_POLICY_SKYVAULT_ODFS_FILE_DELETER_H_
