// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_POLICY_IMPL_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_POLICY_IMPL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_impl.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace enterprise_connectors {
class FileTransferAnalysisDelegate;
}  // namespace enterprise_connectors

namespace file_manager::io_task {

// This class represents a copy or move operation with enabled scanning through
// the OnFileTransferEnterpriseConnector policy.
// This class performs enterprise connector checks for each source file system
// url.
//
// For `settings.block_until_verdict == kBlock`, scans are performed before the
// copy/move operation is started. The scanning results are then used during the
// transfer to block specific files, i.e., when they contain malware or
// sensitive data.
//
// For `settings.block_until_verdict == kNoBlock`, scans are performed after the
// copy/move operation has completed and the results are only used for
// reporting. This is done to minimize the influence of the scan to the user
// experience. As the source might no longer exist after the scan, e.g., because
// the operation was a move, the files are scanned at the destination.
class CopyOrMoveIOTaskPolicyImpl : public CopyOrMoveIOTaskImpl {
  using IsTransferAllowedCallback = base::OnceCallback<void(base::File::Error)>;

 public:
  // `type` must be either kCopy or kMove. The size of `source_urls` and
  // `destination_file_names` must be the same. `settings` should be the
  // settings returned by `FileTransferAnalysisDelegate::IsEnabledVec()` and
  // contain separate settings for each source url. A setting for a source url
  // can be null if scanning is not enabled for that source url.
  CopyOrMoveIOTaskPolicyImpl(
      OperationType type,
      ProgressStatus& progress,
      std::vector<base::FilePath> destination_file_names,
      std::vector<std::optional<enterprise_connectors::AnalysisSettings>>
          settings,
      storage::FileSystemURL destination_folder,
      Profile* profile,
      scoped_refptr<storage::FileSystemContext> file_system_context,
      bool show_notification = true);
  ~CopyOrMoveIOTaskPolicyImpl() override;

  // CopyOrMoveIOTaskImpl overrides:
  void Execute(ProgressCallback progress_callback,
               CompleteCallback complete_callback) override;
  void Resume(ResumeParams params) override;
  void Complete(State state) override;

 private:
  // CopyOrMoveIOTaskImpl overrides:
  // Verifies the transfer by applying Data Leak Prevention files restrictions
  // and enterprise connectors scans.
  void VerifyTransfer() override;
  storage::FileSystemOperation::ErrorBehavior GetErrorBehavior() override;
  std::unique_ptr<storage::CopyOrMoveHookDelegate> GetHookDelegate(
      size_t idx) override;

  // This function scans the source associated with `idx` if scanning is enabled
  // for the respective source-destination-pair.
  // Scanning is always disabled if the source and destination reside on the
  // same volume.
  // For scanning to be enabled, the OnFileTransferEnterpriseConnector policy
  // has to match the source-destination-pair.
  // Scanning is performed recursively for all files within
  // `progress_.sources[idx]`.
  void MaybeScanForDisallowedFiles(size_t idx);

  // Called when scanning is completed.
  void ScanningCompleted();

  // Shows a warning for the connectors.
  // Returns whether the warning was shown.
  bool MaybeShowConnectorsWarning();

  // Called after the warning dialog is proceeded or cancelled.
  // This resumes the transfer and allows for the warned files to be transferred
  // if the warning is proceeded.
  void OnConnectorsWarnDialogResult(
      std::optional<std::u16string> user_justification,
      bool should_proceed);

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

  // Continues executing the IO task after DLP checks are done.
  void OnCheckIfTransferAllowed(
      std::vector<storage::FileSystemURL> blocked_entries);

  // Returns the total number of files in `connectors_blocked_files_`.
  size_t GetConnectorsBlockedFilesNum() const;

  raw_ptr<Profile> profile_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;

  // Stores the settings, only valid until creation of respective.
  // FileTransferAnalysisDelegate.
  std::vector<std::optional<enterprise_connectors::AnalysisSettings>> settings_;
  // Stores the delegates responsible for the file scanning.
  // Will be empty if the FileTransferConnector is disabled. If scanning is
  // disabled for a source-destination-pair, the unique_ptr will be nullptr. If
  // scanning is enabled, a FileTransferAnalysisDelegate will be created.
  std::vector<
      std::unique_ptr<enterprise_connectors::FileTransferAnalysisDelegate>>
      file_transfer_analysis_delegates_;

  // Specifies whether scanning should be used only for reporting.
  // This is set to true if `block_until_verdict` is 0.
  bool report_only_scans_ = false;

  // The list of files blocked by Data Leak Prevention policy.
  std::set<base::FilePath> dlp_blocked_files_;

  // Maps block reasons to their associated enterprise connector blocked file
  // paths.
  std::map<policy::FilesPolicyDialog::BlockReason, std::vector<base::FilePath>>
      connectors_blocked_files_;

  base::WeakPtrFactory<CopyOrMoveIOTaskPolicyImpl> weak_ptr_factory_{this};
};

}  // namespace file_manager::io_task

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_COPY_OR_MOVE_IO_TASK_POLICY_IMPL_H_
