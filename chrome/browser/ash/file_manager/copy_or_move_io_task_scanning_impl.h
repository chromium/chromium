// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_SCANNING_IMPL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_SCANNING_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/files/file.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace enterprise_connectors {
class FileTransferAnalysisDelegate;
}  // namespace enterprise_connectors

namespace file_manager::io_task {

// This class represents a copy or move operation with enabled scanning through
// the OnFileTransferEnterpriseConnector policy.
// This class performs enterprise connector checks for each source file system
// url.
// Scans are performed before the copy/move operation is started. The scanning
// results are then used during the transfer to block specific files, i.e.,
// when they contain malware or sensitive data.
class CopyOrMoveIOTaskScanningImpl : public CopyOrMoveIOTaskImpl {
  using IsTransferAllowedCallback = base::OnceCallback<void(base::File::Error)>;

 public:
  using FileTransferAnalysisDelegateFactory = base::RepeatingCallback<
      std::unique_ptr<enterprise_connectors::FileTransferAnalysisDelegate>(
          safe_browsing::DeepScanAccessPoint access_point,
          storage::FileSystemURL source_url,
          storage::FileSystemURL destination_url,
          Profile* profile,
          storage::FileSystemContext* file_system_context,
          enterprise_connectors::AnalysisSettings settings,
          base::OnceClosure result_callback)>;

  // `type` must be either kCopy or kMove.
  // Use this constructor if you require the destination entries to have
  // different file names to the source entries. The size of `source_urls` and
  // `destination_file_names` must be the same.
  // `settings` should be the settings returned by
  // `FileTransferAnalysisDelegate::IsEnabledVec()` and contain separate
  // settings for each source url. A setting for a source url can be null if
  // scanning is not enabled for that source url.
  CopyOrMoveIOTaskScanningImpl(
      OperationType type,
      ProgressStatus& progress,
      std::vector<base::FilePath> destination_file_names,
      std::vector<absl::optional<enterprise_connectors::AnalysisSettings>>
          settings,
      storage::FileSystemURL destination_folder,
      Profile* profile,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      bool show_notification = true);
  ~CopyOrMoveIOTaskScanningImpl() override;

  static void SetFileTransferAnalysisDelegateFactoryForTesting(
      FileTransferAnalysisDelegateFactory factory);

 private:
  // Verifies the transfer by performing enterprise connector scans.
  void VerifyTransfer() override;

  // This function scans the source associated with `idx` if scanning is enabled
  // for the respective source-destination-pair.
  // Scanning is always disabled if the source and destination reside on the
  // same volume.
  // For scanning to be enabled, the OnFileTransferEnterpriseConnector policy
  // has to match the source-destination-pair.
  // Scanning is performed recursively for all files within
  // `progress_.sources[idx]`.
  void MaybeScanForDisallowedFiles(size_t idx);

  // Checks `file_transfer_analysis_delegates_[idx]` whether a transfer is
  // allowed for the source-destination-pair.
  // If it is allowed, callback will be called with `base::File::FILE_OK`.
  // Otherwise, the callback is run with `base::File::FILE_ERROR_SECURITY`.
  // Note: This function is only allowed to be called if scanning was performed
  // for `idx`.
  void IsTransferAllowed(size_t idx,
                         const storage::FileSystemURL& source_url,
                         const storage::FileSystemURL& destination_url,
                         IsTransferAllowedCallback callback);

  // Returns the error behavior to be used for the copy or move operation.
  storage::FileSystemOperation::ErrorBehavior GetErrorBehavior() override;
  // Returns the storage::CopyOrMoveHookDelegate to be used for the copy or move
  // operation.
  std::unique_ptr<storage::CopyOrMoveHookDelegate> GetHookDelegate(
      size_t idx) override;

  Profile* profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Stores the settings, only valid until creation of respective.
  // FileTransferAnalysisDelegate.
  std::vector<absl::optional<enterprise_connectors::AnalysisSettings>>
      settings_;
  // Stores the delegates responsible for the file scanning.
  // Will be empty if the FileTransferConnector is disabled. If scanning is
  // disabled for a source-destination-pair, the unique_ptr will be nullptr. If
  // scanning is enabled, a FileTransferAnalysisDelegate will be created.
  std::vector<
      std::unique_ptr<enterprise_connectors::FileTransferAnalysisDelegate>>
      file_transfer_analysis_delegates_;

  base::WeakPtrFactory<CopyOrMoveIOTaskScanningImpl> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_SCANNING_IMPL_H_
