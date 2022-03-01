// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_EXTRACT_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_EXTRACT_IO_TASK_H_

#include <vector>

#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager {
namespace io_task {

class ExtractIOTask : public IOTask {
 public:
  // Create a task to extract any ZIP files in |source_urls|. These
  // must be under the |parent_folder| directory, and the resulting extraction
  // will be created there.
  ExtractIOTask(std::vector<storage::FileSystemURL> source_urls,
                storage::FileSystemURL parent_folder,
                scoped_refptr<storage::FileSystemContext> file_system_context);
  ~ExtractIOTask() override;

  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;

  void Cancel() override;

 private:
  void Complete(State state);

  const scoped_refptr<storage::FileSystemContext> file_system_context_;

  ProgressCallback progress_callback_;
  CompleteCallback complete_callback_;

  base::WeakPtrFactory<ExtractIOTask> weak_ptr_factory_{this};
};

}  // namespace io_task
}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_EXTRACT_IO_TASK_H_
