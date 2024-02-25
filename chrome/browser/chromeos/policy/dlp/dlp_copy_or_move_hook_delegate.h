// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_COPY_OR_MOVE_HOOK_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_COPY_OR_MOVE_HOOK_DELEGATE_H_

#include <absl/container/flat_hash_map.h>
#include <memory>

#include "base/files/file_path.h"
#include "base/memory/weak_ptr.h"
#include "components/file_access/scoped_file_access.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy {

class DlpCopyOrMoveHookDelegateTest;

// Providing hooks called from //storage on IO threads. Calls are redirected on
// the UI thread to use DlpFilesController.
class DlpCopyOrMoveHookDelegate : public storage::CopyOrMoveHookDelegate {
 public:
  explicit DlpCopyOrMoveHookDelegate(bool isComposite = false);

  ~DlpCopyOrMoveHookDelegate() override;

  // Callback after access for the copy operation is granted. It takes care that
  // the ScopedFileAccess is kept for the whole copy operation.
  void GotAccess(const storage::FileSystemURL& source,
                 const storage::FileSystemURL& destination,
                 std::unique_ptr<file_access::ScopedFileAccess> access);

  // storage::CopyOrMoveHookDelegate:
  void OnBeginProcessFile(const storage::FileSystemURL& source_url,
                          const storage::FileSystemURL& destination_url,
                          StatusCallback callback) override;
  void OnEndCopy(const storage::FileSystemURL& source_url,
                 const storage::FileSystemURL& destination_url) override;
  void OnEndMove(const storage::FileSystemURL& source_url,
                 const storage::FileSystemURL& destination_url) override;
  void OnError(const storage::FileSystemURL& source_url,
               const storage::FileSystemURL& destination_url,
               base::File::Error error,
               ErrorCallback callback) override;

 private:
  void OnEnd(const storage::FileSystemURL& source_url,
             const storage::FileSystemURL& destination_url);

  friend DlpCopyOrMoveHookDelegateTest;

  // Keeps the ScopedFileAccess object for the whole copy operation between the
  // two paths. The key consists of the <source path, destination path>.
  absl::flat_hash_map<std::pair<base::FilePath, base::FilePath>,
                      std::unique_ptr<file_access::ScopedFileAccess>>
      current_access_map_;

  base::WeakPtrFactory<DlpCopyOrMoveHookDelegate> weak_ptr_factory_{this};
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_COPY_OR_MOVE_HOOK_DELEGATE_H_
