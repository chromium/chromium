// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_ENCRYPTED_HOOK_DELEGATE_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_ENCRYPTED_HOOK_DELEGATE_H_

#include "base/functional/callback.h"
#include "chromeos/ash/components/drivefs/mojom/drivefs.mojom-forward.h"
#include "components/drive/file_errors.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_url.h"

class Profile;

namespace file_manager::io_task {

// A delegate for copy or move storage operations which detects failed attempts
// to read encrypted (Google Drive CSE) files, skips them to continue the
// operation and reports them to the caller via the provided callback
// (`on_file_skipped` in the constructor).
//
// Threading: The class should be created on the UI thread, the provided
// `on_file_skipped` callback will be also called on the UI thread, but the
// `OnError` method and destruction are called by the copy or move operation,
// which happens on the IO thread.
class CopyOrMoveEncryptedHookDelegate : public storage::CopyOrMoveHookDelegate {
 public:
  CopyOrMoveEncryptedHookDelegate(
      Profile* profile,
      base::RepeatingCallback<void(storage::FileSystemURL source_url)>
          on_file_skipped);
  ~CopyOrMoveEncryptedHookDelegate() override;

 private:
  // storage::CopyOrMoveHookDelegate
  void OnError(const storage::FileSystemURL& source_url,
               const storage::FileSystemURL& destination_url,
               base::File::Error error,
               ErrorCallback callback) override;

  base::RepeatingCallback<void(storage::FileSystemURL source_url)>
      on_file_skipped_;
  base::RepeatingCallback<void(
      storage::FileSystemURL source_url,
      base::RepeatingCallback<void(storage::FileSystemURL source_url)>
          skip_callback,
      ErrorCallback finish_callback)>
      check_file_;
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_ENCRYPTED_HOOK_DELEGATE_H_
