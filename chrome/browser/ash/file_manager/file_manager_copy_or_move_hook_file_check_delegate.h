// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_COPY_OR_MOVE_HOOK_FILE_CHECK_DELEGATE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_COPY_OR_MOVE_HOOK_FILE_CHECK_DELEGATE_H_

#include "base/files/file.h"
#include "base/functional/callback_forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"

namespace file_manager {

// FileManagerCopyOrMoveHookFileCheckDelegate queries `file_check_callback_` to
// check if a certain file is allowed to be copied or moved and
// notifies the copy or move operation using the provided `callback`.
class FileManagerCopyOrMoveHookFileCheckDelegate
    : public storage::CopyOrMoveHookDelegate {
 public:
  using FileCheckCallback = base::RepeatingCallback<void(
      const storage::FileSystemURL&,
      const storage::FileSystemURL&,
      storage::FileSystemOperation::StatusCallback)>;

  FileManagerCopyOrMoveHookFileCheckDelegate(
      scoped_refptr<storage::FileSystemContext> file_system_context,
      FileCheckCallback file_check_callback);

  ~FileManagerCopyOrMoveHookFileCheckDelegate() override;

  void OnBeginProcessFile(
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url,
      storage::FileSystemOperation::StatusCallback callback) override;

 private:
  void OnBeginProcessFileGotMetadata(
      const storage::FileSystemURL& src_url,
      const storage::FileSystemURL& dest_url,
      storage::FileSystemOperation::StatusCallback callback,
      base::File::Error result,
      const base::File::Info& file_info);

  scoped_refptr<storage::FileSystemContext> file_system_context_;
  FileCheckCallback file_check_callback_;

  base::WeakPtrFactory<FileManagerCopyOrMoveHookFileCheckDelegate>
      weak_factory_{this};
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_COPY_OR_MOVE_HOOK_FILE_CHECK_DELEGATE_H_
