// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_io_task_scanning_impl.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/no_destructor.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_file_check_delegate.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace file_manager::io_task {

namespace {

CopyOrMoveIOTaskScanningImpl::FileTransferAnalysisDelegateFactory&
GetFactoryStorage() {
  static base::NoDestructor<
      CopyOrMoveIOTaskScanningImpl::FileTransferAnalysisDelegateFactory>
      factory;
  return *factory;
}

}  // namespace

CopyOrMoveIOTaskScanningImpl::CopyOrMoveIOTaskScanningImpl(
    OperationType type,
    ProgressStatus& progress,
    std::vector<base::FilePath> destination_file_names,
    std::vector<absl::optional<enterprise_connectors::AnalysisSettings>>
        settings,
    storage::FileSystemURL destination_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    bool show_notification)
    : CopyOrMoveIOTaskImpl(type,
                           progress,
                           std::move(destination_file_names),
                           std::move(destination_folder),
                           profile,
                           file_system_context,
                           show_notification),
      profile_(profile),
      file_system_context_(file_system_context),
      settings_(std::move(settings)) {}

CopyOrMoveIOTaskScanningImpl::~CopyOrMoveIOTaskScanningImpl() = default;

// static
void CopyOrMoveIOTaskScanningImpl::
    SetFileTransferAnalysisDelegateFactoryForTesting(
        FileTransferAnalysisDelegateFactory factory) {
  GetFactoryStorage() = factory;
}

void CopyOrMoveIOTaskScanningImpl::VerifyTransfer() {
  // Allocate one unique_ptr for each source. If it is not set, scanning is not
  // enabled for this source.
  file_transfer_analysis_delegates_.resize(progress_.sources.size());
  MaybeScanForDisallowedFiles(0);
}

void CopyOrMoveIOTaskScanningImpl::MaybeScanForDisallowedFiles(size_t idx) {
  DCHECK_LE(idx, progress_.sources.size());
  if (idx == progress_.sources.size()) {
    // Scanning is complete.
    StartTransfer();
    return;
  }
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!settings_[idx].has_value()) {
    // Skip checking if connectors aren't enabled.
    MaybeScanForDisallowedFiles(idx + 1);
    return;
  }

  if (progress_.state != State::kScanning) {
    progress_.state = State::kScanning;
    progress_callback_.Run(progress_);
  }

  DCHECK_EQ(file_transfer_analysis_delegates_.size(), progress_.sources.size());

  if (GetFactoryStorage().is_null()) {
    // This code path is always reached outside of tests.
    file_transfer_analysis_delegates_[idx] =
        std::make_unique<enterprise_connectors::FileTransferAnalysisDelegate>(
            safe_browsing::DeepScanAccessPoint::FILE_TRANSFER,
            progress_.sources[idx].url, progress_.destination_folder, profile_,
            file_system_context_.get(), std::move(settings_[idx].value()),
            base::BindOnce(
                &CopyOrMoveIOTaskScanningImpl::MaybeScanForDisallowedFiles,
                weak_ptr_factory_.GetWeakPtr(), idx + 1));
  } else {
    // Only in tests, GetFactoryStorage() can be set and this code path can be
    // reached.
    // Pass `idx` in addition to the constructor parameters to make testing
    // easier.
    file_transfer_analysis_delegates_[idx] = GetFactoryStorage().Run(
        safe_browsing::DeepScanAccessPoint::FILE_TRANSFER,
        progress_.sources[idx].url, progress_.destination_folder, profile_,
        file_system_context_.get(), std::move(settings_[idx].value()),
        base::BindOnce(
            &CopyOrMoveIOTaskScanningImpl::MaybeScanForDisallowedFiles,
            weak_ptr_factory_.GetWeakPtr(), idx + 1));
  }
  file_transfer_analysis_delegates_[idx]->UploadData();
}

void CopyOrMoveIOTaskScanningImpl::IsTransferAllowed(
    size_t idx,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    IsTransferAllowedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(file_transfer_analysis_delegates_[idx]);
  auto result =
      file_transfer_analysis_delegates_[idx]->GetAnalysisResultAfterScan(
          source_url);
  if (result ==
      enterprise_connectors::FileTransferAnalysisDelegate::RESULT_ALLOWED) {
    std::move(callback).Run(base::File::FILE_OK);
    return;
  }
  DCHECK(
      result ==
          enterprise_connectors::FileTransferAnalysisDelegate::RESULT_UNKNOWN ||
      result ==
          enterprise_connectors::FileTransferAnalysisDelegate::RESULT_BLOCKED);

  std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
}

storage::FileSystemOperation::ErrorBehavior
CopyOrMoveIOTaskScanningImpl::GetErrorBehavior() {
  // For the enterprise connectors, we want files to be copied/moved if they are
  // allowed and files to be prevented from copying/moving if they are blocked.
  // With `ERROR_BEHAVIOR_ABORT`, the first blocked file would result in the
  // copy/move operation to be aborted.
  // With `ERROR_BEHAVIOR_SKIP`, blocked files are ignored and all allowed files
  // will be copied.
  return storage::FileSystemOperation::ERROR_BEHAVIOR_SKIP;
}

std::unique_ptr<storage::CopyOrMoveHookDelegate>
CopyOrMoveIOTaskScanningImpl::GetHookDelegate(size_t idx) {
  DCHECK_LT(idx, file_transfer_analysis_delegates_.size());

  // For all callbacks, we are using CreateRelayCallback to ensure that the
  // callbacks are executed on the current (i.e., UI) thread.
  auto progress_callback = google_apis::CreateRelayCallback(
      base::BindRepeating(&CopyOrMoveIOTaskScanningImpl::OnCopyOrMoveProgress,
                          weak_ptr_factory_.GetWeakPtr()));

  if (!file_transfer_analysis_delegates_[idx]) {
    // If scanning is disabled, use the normal delegate.
    // This can happen if some source_urls lie on a file system for which
    // scanning is enabled, while other source_urls lie on a file system for
    // which scanning is disabled.
    return std::make_unique<FileManagerCopyOrMoveHookDelegate>(
        progress_callback);
  }

  auto file_check_callback = google_apis::CreateRelayCallback(
      base::BindRepeating(&CopyOrMoveIOTaskScanningImpl::IsTransferAllowed,
                          weak_ptr_factory_.GetWeakPtr(), idx));
  return std::make_unique<FileManagerCopyOrMoveHookFileCheckDelegate>(
      file_system_context_, progress_callback, file_check_callback);
}

}  // namespace file_manager::io_task
