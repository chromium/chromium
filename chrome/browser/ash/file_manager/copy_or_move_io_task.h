// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_H_

#include <memory>
#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_impl.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/profiles/profile.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager::io_task {

// This class represents a copy or move operation. It checks whether there is
// enough space for the copy or move to occur, and also sends the copy or move
// requests to the storage backend.
class CopyOrMoveIOTask : public IOTask {
 public:
  // |type| must be either kCopy or kMove.
  CopyOrMoveIOTask(
      OperationType type,
      std::vector<storage::FileSystemURL> source_urls,
      storage::FileSystemURL destination_folder,
      Profile* profile,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      bool show_notification = true);
  // Use this constructor if you require the destination entries to have
  // different file names to the source entries. The size of `source_urls` and
  // `destination_file_names` must be the same.
  CopyOrMoveIOTask(
      OperationType type,
      std::vector<storage::FileSystemURL> source_urls,
      std::vector<base::FilePath> destination_file_names,
      storage::FileSystemURL destination_folder,
      Profile* profile,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      bool show_notification = true);
  ~CopyOrMoveIOTask() override;

  // Starts the copy or move.
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;

  // Pauses the copy or move.
  void Pause(PauseParams params) override;

  // Resumes the copy or move.
  void Resume(ResumeParams params) override;

  // Cancels the copy or move.
  void Cancel() override;

  // Aborts the copy or move because of policy error.
  void CompleteWithError(PolicyError policy_error) override;

 private:
  raw_ptr<Profile> profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  std::vector<storage::FileSystemURL> source_urls_;
  // Stores a list of file names (i.e. base::FilePath::BaseName, not full paths)
  // that will serve as the name for the source URLs in progress_.sources. These
  // names are prior to conflict resolution so in the event they conflict they
  // may be renamed to include a numbered suffix (e.g. foo.txt (1)). The
  // std::vector::size here MUST be the same as progress_.sources size.
  std::vector<base::FilePath> destination_file_names_;

  std::unique_ptr<CopyOrMoveIOTaskImpl> impl_;

  base::WeakPtrFactory<CopyOrMoveIOTask> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_H_
