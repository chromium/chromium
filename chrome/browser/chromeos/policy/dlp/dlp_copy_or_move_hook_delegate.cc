// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_copy_or_move_hook_delegate.h"

#include <memory>
#include <utility>

#include "base/callback_list.h"
#include "base/check.h"
#include "base/files/file.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/memory/weak_ptr.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/threading/thread_checker.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_files_controller.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "components/file_access/scoped_file_access.h"
#include "components/file_access/scoped_file_access_delegate.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_url.h"

namespace policy {
namespace {

void GotAccess(base::WeakPtr<DlpCopyOrMoveHookDelegate> hook_delegate,
               const storage::FileSystemURL& source,
               const storage::FileSystemURL& destination,
               DlpCopyOrMoveHookDelegate::StatusCallback callback,
               std::unique_ptr<file_access::ScopedFileAccess> access) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  bool is_allowed = access->is_allowed();
  if (hook_delegate.MaybeValid()) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&DlpCopyOrMoveHookDelegate::GotAccess, hook_delegate,
                       source, destination, std::move(access)));
  }
  // The `callback` was bound to the calling thread in OnBeginProcessFile and
  // will be executed on the IO thread.
  if (is_allowed) {
    std::move(callback).Run(base::File::FILE_OK);
  } else {
    std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
  }
}

void RequestCopyAccess(base::WeakPtr<DlpCopyOrMoveHookDelegate> hook_delegate,
                       const storage::FileSystemURL& source,
                       const storage::FileSystemURL& destination,
                       DlpCopyOrMoveHookDelegate::StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  DlpRulesManager* dlp_rules_manager =
      DlpRulesManagerFactory::GetForPrimaryProfile();
  if (!dlp_rules_manager) {
    std::move(callback).Run(base::File::FILE_OK);
    return;
  }
  DlpFilesController* dlp_files_controller =
      dlp_rules_manager->GetDlpFilesController();
  if (!dlp_files_controller) {
    std::move(callback).Run(base::File::FILE_OK);
    return;
  }
  dlp_files_controller->RequestCopyAccess(
      source, destination,
      base::BindOnce(&policy::GotAccess, hook_delegate, source, destination,
                     std::move(callback)));
}

}  // namespace

DlpCopyOrMoveHookDelegate::DlpCopyOrMoveHookDelegate(bool isComposite)
    : CopyOrMoveHookDelegate(isComposite) {}

DlpCopyOrMoveHookDelegate::~DlpCopyOrMoveHookDelegate() = default;

void DlpCopyOrMoveHookDelegate::GotAccess(
    const storage::FileSystemURL& source,
    const storage::FileSystemURL& destination,
    std::unique_ptr<file_access::ScopedFileAccess> access) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  current_access_map_.emplace(std::make_pair(source.path(), destination.path()),
                              std::move(access));
}

void DlpCopyOrMoveHookDelegate::OnBeginProcessFile(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    StatusCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  StatusCallback continuation =
      base::BindPostTaskToCurrentDefault(std::move(callback));
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&RequestCopyAccess, weak_ptr_factory_.GetWeakPtr(),
                     source_url, destination_url, std::move(continuation)));
}

void DlpCopyOrMoveHookDelegate::OnEndCopy(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  OnEnd(source_url, destination_url);
}

void DlpCopyOrMoveHookDelegate::OnEndMove(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  OnEnd(source_url, destination_url);
}

void DlpCopyOrMoveHookDelegate::OnError(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    base::File::Error error,
    ErrorCallback callback) {
  OnEnd(source_url, destination_url);
  std::move(callback).Run(ErrorAction::kDefault);
}

void DlpCopyOrMoveHookDelegate::OnEnd(
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  current_access_map_.erase(
      std::make_pair(source_url.path(), destination_url.path()));
}

}  // namespace policy
