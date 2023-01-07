// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_COPY_OR_MOVE_HOOK_DELEGATE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_COPY_OR_MOVE_HOOK_DELEGATE_H_

#include "storage/browser/file_system/copy_or_move_hook_delegate.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy {

// Providing hooks called from //storage on IO threads. Calls are redirected on
// the UI thread to use DlpFilesController.
class DlpCopyOrMoveHookDelegate : public storage::CopyOrMoveHookDelegate {
 public:
  explicit DlpCopyOrMoveHookDelegate(bool isComposite = false);

  ~DlpCopyOrMoveHookDelegate() override;

  void OnEndCopy(const storage::FileSystemURL& source_url,
                 const storage::FileSystemURL& destination_url) override;

  void OnEndMove(const storage::FileSystemURL& source_url,
                 const storage::FileSystemURL& destination_url) override;

 private:
  void OnSuccess(const storage::FileSystemURL& source_url,
                 const storage::FileSystemURL& destination_url);
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_COPY_OR_MOVE_HOOK_DELEGATE_H_
