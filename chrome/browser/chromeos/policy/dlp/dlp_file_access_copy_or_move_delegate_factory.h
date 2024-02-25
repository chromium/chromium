// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILE_ACCESS_COPY_OR_MOVE_DELEGATE_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILE_ACCESS_COPY_OR_MOVE_DELEGATE_FACTORY_H_

#include "chrome/browser/chromeos/policy/dlp/dlp_scoped_file_access_delegate.h"
#include "components/file_access/file_access_copy_or_move_delegate_factory.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate.h"

namespace policy {

// Implements the factory pattern to create CopyOrMoveHookDelegate objects
// ensuring the DLP restrictions applied to copy and move operations. For local
// copies (MyFiles / Downloads) the source URL information of restricted files
// is copied beside the files. The object lives on the IO thread so MakeHook()
// should only be called from there. Initialize() and DeleteInstance() can
// also be called from the UI thread as the calls get redirected.
class DlpFileAccessCopyOrMoveDelegateFactory
    : public file_access::FileAccessCopyOrMoveDelegateFactory {
 public:
  std::unique_ptr<storage::CopyOrMoveHookDelegate> MakeHook() override;

 protected:
  friend DlpScopedFileAccessDelegate;
  friend class DlpFileAccessCopyOrMoveDelegateFactoryTest;
  static void Initialize();
  static void DeleteInstance();
};

}  // namespace policy
#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_FILE_ACCESS_COPY_OR_MOVE_DELEGATE_FACTORY_H_
