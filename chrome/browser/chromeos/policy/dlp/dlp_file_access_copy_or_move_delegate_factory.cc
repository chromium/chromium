// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_file_access_copy_or_move_delegate_factory.h"

#include <memory>

#include "chrome/browser/chromeos/policy/dlp/dlp_copy_or_move_hook_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace policy {

std::unique_ptr<storage::CopyOrMoveHookDelegate>
DlpFileAccessCopyOrMoveDelegateFactory::MakeHook() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return std::make_unique<DlpCopyOrMoveHookDelegate>();
}

// static
void DlpFileAccessCopyOrMoveDelegateFactory::Initialize() {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce([] { new DlpFileAccessCopyOrMoveDelegateFactory(); }));
}

// static
void DlpFileAccessCopyOrMoveDelegateFactory::DeleteInstance() {
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(
          &::file_access::FileAccessCopyOrMoveDelegateFactory::DeleteInstance));
}

}  // namespace policy
