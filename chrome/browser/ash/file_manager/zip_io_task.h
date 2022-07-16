// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_ZIP_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_ZIP_IO_TASK_H_

#include <vector>

#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/speedometer.h"
#include "chrome/services/file_util/public/cpp/zip_file_creator.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager {
namespace io_task {

class ZipIOTask : public IOTask {
 public:
  // Create a task to zip the given files and folders in |source_urls|. These
  // must be under the |parent_folder| directory, and the resulting ZIP file
  // will be created there. If there is only one file to zip, that file will be
  // used as the filename of the archive. Otherwise 'Archive.zip' will be used.
  ZipIOTask(std::vector<storage::FileSystemURL> source_urls,
            storage::FileSystemURL parent_folder,
            scoped_refptr<storage::FileSystemContext> file_system_context);
  ~ZipIOTask() override;

  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;

  void Cancel() override;

 private:
  void Complete(State state);
  void GenerateZipNameAfterGotTotalBytes(int64_t total_bytes);
  void ZipItems(base::FileErrorOr<storage::FileSystemURL> dest_result);
  void OnZipProgress();
  void OnZipComplete();

  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // The directory containing the files to zip.
  base::FilePath source_dir_;

  // A list of paths relative to the source directory of files to zip.
  std::vector<base::FilePath> source_relative_paths_;

  scoped_refptr<ZipFileCreator> zip_file_creator_;

  // Speedometer for this operation, used to calculate the remaining time to
  // finish the operation.
  Speedometer speedometer_;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<ZipIOTask> weak_ptr_factory_{this};
};

}  // namespace io_task
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_ZIP_IO_TASK_H_
