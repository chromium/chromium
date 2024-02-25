// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_COPY_OR_MOVE_HOOK_DELEGATE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_COPY_OR_MOVE_HOOK_DELEGATE_H_

#include <iosfwd>

#include "base/files/file.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_operation.h"

namespace storage {
class FileSystemURL;
}  // namespace storage

namespace file_manager {

// FileManagerCopyOrMoveHookDelegate uses the ProgressCallback to propagate the
// different progresses to the files app.
// OnBeginProcessFile and OnBeginProcessDirectory both pass kBegin to the
// callback.
class FileManagerCopyOrMoveHookDelegate
    : public storage::CopyOrMoveHookDelegate {
 public:
  enum class ProgressType {
    kBegin = 0,
    kProgress,
    kEndCopy,
    kEndMove,
    kEndRemoveSource,
    kError,
  };

  using ProgressCallback = base::RepeatingCallback<void(
      ProgressType type,
      const storage::FileSystemURL& source_url,
      const storage::FileSystemURL& destination_url,
      int64_t size)>;

  explicit FileManagerCopyOrMoveHookDelegate(
      ProgressCallback progress_callback);

  ~FileManagerCopyOrMoveHookDelegate() override;

  void OnBeginProcessFile(const storage::FileSystemURL& source_url,
                          const storage::FileSystemURL& destination_url,
                          StatusCallback callback) override;

  void OnBeginProcessDirectory(const storage::FileSystemURL& source_url,
                               const storage::FileSystemURL& destination_url,
                               StatusCallback callback) override;

  void OnProgress(const storage::FileSystemURL& source_url,
                  const storage::FileSystemURL& destination_url,
                  int64_t size) override;

  void OnError(const storage::FileSystemURL& source_url,
               const storage::FileSystemURL& destination_url,
               base::File::Error error,
               ErrorCallback callback) override;

  void OnEndCopy(const storage::FileSystemURL& source_url,
                 const storage::FileSystemURL& destination_url) override;

  void OnEndMove(const storage::FileSystemURL& source_url,
                 const storage::FileSystemURL& destination_url) override;

  void OnEndRemoveSource(const storage::FileSystemURL& source_url) override;

 protected:
  ProgressCallback GUARDED_BY_CONTEXT(sequence_checker_) progress_callback_;
};

// Out operator for logging FileManagerCopyOrMoveHookDelegate::Progress type.
std::ostream& operator<<(
    std::ostream& out,
    const FileManagerCopyOrMoveHookDelegate::ProgressType& type);

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_FILE_MANAGER_COPY_OR_MOVE_HOOK_DELEGATE_H_
