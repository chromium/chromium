// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_copy_or_move_hook_delegate.h"

#include "base/check.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy {
namespace {

void CopySourceInformation(storage::FileSystemURL source,
                           storage::FileSystemURL destination) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  DlpRulesManager* rules_manager;
  rules_manager = DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!rules_manager) {
    return;
  }
  DlpFilesController* controller = rules_manager->GetDlpFilesController();
  if (!controller) {
    return;
  }
  controller->CopySourceInformation(source, destination);
#else
  NOTREACHED();
#endif
}

}  // namespace

DlpCopyOrMoveHookDelegate::DlpCopyOrMoveHookDelegate(bool isComposite)
    : CopyOrMoveHookDelegate(isComposite) {}

DlpCopyOrMoveHookDelegate::~DlpCopyOrMoveHookDelegate() = default;

void DlpCopyOrMoveHookDelegate::OnEndCopy(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  OnSuccess(source_url, destination_url);
}

void DlpCopyOrMoveHookDelegate::OnEndMove(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  OnSuccess(source_url, destination_url);
}

void DlpCopyOrMoveHookDelegate::OnSuccess(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&CopySourceInformation, source_url, destination_url));
}

}  // namespace policy
