// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_io_task_policy_impl.h"

#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_impl.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_file_check_delegate.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"
#include "chrome/browser/enterprise/data_controls/component.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace file_manager::io_task {

namespace {

// Scan the `idx`-th entry.
//
// Note: We include the previous_file_transfer_analysis_delegate here to manage
// its lifetime.
void DoReportOnlyScanning(
    std::unique_ptr<enterprise_connectors::FileTransferAnalysisDelegate>
        previous_file_transfer_analysis_delegate,
    size_t idx,
    std::vector<absl::optional<enterprise_connectors::AnalysisSettings>>
        settings,
    std::vector<storage::FileSystemURL> sources,
    std::vector<storage::FileSystemURL> outputs,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context) {
  previous_file_transfer_analysis_delegate.reset();

  if (idx >= settings.size()) {
    // Scanning is complete!
    return;
  }

  if (!settings[idx].has_value()) {
    // Don't scan this entry, but try the next.
    DoReportOnlyScanning(nullptr, idx + 1, std::move(settings),
                         std::move(sources), std::move(outputs), profile,
                         file_system_context);
    return;
  }

  std::unique_ptr<enterprise_connectors::FileTransferAnalysisDelegate>
      file_transfer_analysis_delegate =
          enterprise_connectors::FileTransferAnalysisDelegate::Create(
              safe_browsing::DeepScanAccessPoint::FILE_TRANSFER, sources[idx],
              outputs[idx], profile, file_system_context.get(),
              std::move(settings[idx].value()));

  // Manage lifetime of the file_transfer_analysis_delegate by binding it to the
  // completion callback.
  auto* file_transfer_analysis_delegate_ptr =
      file_transfer_analysis_delegate.get();
  file_transfer_analysis_delegate_ptr->UploadData(base::BindOnce(
      &DoReportOnlyScanning, std::move(file_transfer_analysis_delegate),
      idx + 1, std::move(settings), std::move(sources), std::move(outputs),
      profile, file_system_context));
}

// Start the asynchronous report-only scans.
//
// The `io_task_completion_callback` will be run before the scans are executed.
void StartReportOnlyScanning(
    IOTask::CompleteCallback io_task_completion_callback,
    std::vector<absl::optional<enterprise_connectors::AnalysisSettings>>
        settings,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    ProgressStatus status) {
  DCHECK_EQ(settings.size(), status.sources.size());
  DCHECK_EQ(settings.size(), status.outputs.size());
  std::vector<storage::FileSystemURL> sources(settings.size());
  std::vector<storage::FileSystemURL> outputs(settings.size());
  for (size_t i = 0; i < settings.size(); ++i) {
    sources[i] = status.sources[i].url;
    outputs[i] = status.outputs[i].url;
  }

  // Notify the Files app of completion of the copy/move.
  std::move(io_task_completion_callback).Run(std::move(status));

  // Start the actual scanning.
  DoReportOnlyScanning(nullptr, 0, std::move(settings), std::move(sources),
                       std::move(outputs), profile, file_system_context);
}

}  // namespace

CopyOrMoveIOTaskPolicyImpl::CopyOrMoveIOTaskPolicyImpl(
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
      settings_(std::move(settings)) {
  if (!settings_.empty()) {
    // The value of `block_until_verdict` is consistent for all settings, so we
    // just check the value for the first valid setting.
    auto valid_setting = base::ranges::find_if(
        settings_,
        [](const absl::optional<enterprise_connectors::AnalysisSettings>&
               setting) { return setting.has_value(); });
    report_only_scans_ = valid_setting->value().block_until_verdict ==
                         enterprise_connectors::BlockUntilVerdict::kNoBlock;
  }
}

CopyOrMoveIOTaskPolicyImpl::~CopyOrMoveIOTaskPolicyImpl() = default;

void CopyOrMoveIOTaskPolicyImpl::Execute(
    IOTask::ProgressCallback progress_callback,
    IOTask::CompleteCallback complete_callback) {
  if (report_only_scans_) {
    // For report only scans, we perform the scans AFTER the transfer. So we
    // wrap the completion callback.
    CopyOrMoveIOTaskImpl::Execute(
        std::move(progress_callback),
        base::BindOnce(&StartReportOnlyScanning, std::move(complete_callback),
                       std::move(settings_), profile_, file_system_context_));
  } else {
    CopyOrMoveIOTaskImpl::Execute(std::move(progress_callback),
                                  std::move(complete_callback));
  }
}

void CopyOrMoveIOTaskPolicyImpl::Resume(ResumeParams params) {
  // In this class we only handle policy resumes, anything else defer to the
  // base class.
  if (!params.policy_params.has_value()) {
    CopyOrMoveIOTaskImpl::Resume(std::move(params));
    return;
  }

  auto* files_policy_manager =
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          profile_);
  if (!files_policy_manager) {
    LOG(ERROR) << "Couldn't find FilesPolicyNotificationManager";
    Complete(State::kError);
    return;
  }

  if (params.policy_params->type == policy::Policy::kDlp ||
      params.policy_params->type == policy::Policy::kEnterpriseConnectors) {
    files_policy_manager->OnIOTaskResumed(progress_->task_id);
  }
}

void CopyOrMoveIOTaskPolicyImpl::MaybeSendConnectorsBlockedFilesNotification() {
  if (connectors_blocked_files_.empty()) {
    return;
  }

  // Blocked files are only added if kFileTransferEnterpriseConnectorUI is
  // enabled.
  CHECK(base::FeatureList::IsEnabled(
      features::kFileTransferEnterpriseConnectorUI));

  auto* files_policy_manager =
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          profile_);
  if (!files_policy_manager) {
    LOG(ERROR) << "Couldn't find FilesPolicyNotificationManager";
    return;
  }
  files_policy_manager->AddConnectorsBlockedFiles(
      progress_->task_id, std::move(connectors_blocked_files_),
      progress_->type == file_manager::io_task::OperationType::kMove
          ? policy::dlp::FileAction::kMove
          : policy::dlp::FileAction::kCopy);
}

void CopyOrMoveIOTaskPolicyImpl::Complete(State state) {
  if (blocked_files_ > 0 &&
      base::FeatureList::IsEnabled(features::kNewFilesPolicyUX)) {
    bool has_dlp_errors = connectors_blocked_files_.size() < blocked_files_;
    bool has_connector_errors = !connectors_blocked_files_.empty();

    CHECK(has_dlp_errors || has_connector_errors);
    // TODO(b/293425493): Support combined error type (if both dlp and connector
    // errors exist).
    PolicyErrorType error_type = has_dlp_errors
                                     ? PolicyErrorType::kDlp
                                     : PolicyErrorType::kEnterpriseConnectors;

    progress_->policy_error.emplace(error_type, blocked_files_,
                                    blocked_file_name_);
    state = State::kError;

    MaybeSendConnectorsBlockedFilesNotification();
  }

  CopyOrMoveIOTaskImpl::Complete(state);
}

void CopyOrMoveIOTaskPolicyImpl::VerifyTransfer() {
  auto on_check_transfer_cb =
      base::BindOnce(&CopyOrMoveIOTaskPolicyImpl::OnCheckIfTransferAllowed,
                     weak_ptr_factory_.GetWeakPtr());

  if (auto* files_controller =
          policy::DlpFilesControllerAsh::GetForPrimaryProfile();
      base::FeatureList::IsEnabled(features::kNewFilesPolicyUX) &&
      files_controller) {
    std::vector<storage::FileSystemURL> transferred_urls;
    for (const auto& entry : progress_->sources) {
      transferred_urls.push_back(entry.url);
    }
    bool is_move =
        progress_->type == file_manager::io_task::OperationType::kMove ? true
                                                                       : false;
    files_controller->CheckIfTransferAllowed(
        progress_->task_id, std::move(transferred_urls),
        progress_->GetDestinationFolder(), is_move,
        std::move(on_check_transfer_cb));
    return;
  }

  std::move(on_check_transfer_cb).Run(/*blocked_entries=*/{});
}

storage::FileSystemOperation::ErrorBehavior
CopyOrMoveIOTaskPolicyImpl::GetErrorBehavior() {
  // This function is called when the transfer starts and DLP restrictions are
  // applied before the transfer. If there's any file blocked by DLP, the error
  // behavior should be skip instead of abort.
  if (report_only_scans_ && !blocked_files_) {
    return storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT;
  }
  // For the enterprise connectors, we want files to be copied/moved if they are
  // allowed and files to be prevented from copying/moving if they are blocked.
  // With `ERROR_BEHAVIOR_ABORT`, the first blocked file would result in the
  // copy/move operation to be aborted.
  // With `ERROR_BEHAVIOR_SKIP`, blocked files are ignored and all allowed files
  // will be copied.
  return storage::FileSystemOperation::ERROR_BEHAVIOR_SKIP;
}

std::unique_ptr<storage::CopyOrMoveHookDelegate>
CopyOrMoveIOTaskPolicyImpl::GetHookDelegate(size_t idx) {
  // For all callbacks, we are using CreateRelayCallback to ensure that the
  // callbacks are executed on the current (i.e., UI) thread.
  auto progress_callback = google_apis::CreateRelayCallback(
      base::BindRepeating(&CopyOrMoveIOTaskPolicyImpl::OnCopyOrMoveProgress,
                          weak_ptr_factory_.GetWeakPtr(), idx));
  if (settings_.empty() || report_only_scans_) {
    // For DLP only restrictions or report-only scans, no blocking should be
    // performed, so we use the normal delegate.
    return std::make_unique<FileManagerCopyOrMoveHookDelegate>(
        progress_callback);
  }

  DCHECK_LT(idx, file_transfer_analysis_delegates_.size());
  if (!file_transfer_analysis_delegates_[idx]) {
    // If scanning is disabled, use the normal delegate.
    // Scanning can be disabled if some source_urls lie on a file system for
    // which scanning is enabled, while other source_urls lie on a file system
    // for which scanning is disabled.
    return std::make_unique<FileManagerCopyOrMoveHookDelegate>(
        progress_callback);
  }

  auto file_check_callback = google_apis::CreateRelayCallback(
      base::BindRepeating(&CopyOrMoveIOTaskPolicyImpl::IsTransferAllowed,
                          weak_ptr_factory_.GetWeakPtr(), idx));
  return std::make_unique<FileManagerCopyOrMoveHookFileCheckDelegate>(
      file_system_context_, progress_callback, file_check_callback);
}

void CopyOrMoveIOTaskPolicyImpl::MaybeScanForDisallowedFiles(size_t idx) {
  DCHECK_LE(idx, progress_->sources.size());
  if (idx == progress_->sources.size()) {
    ScanningCompleted();
    return;
  }
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!settings_[idx].has_value()) {
    // Skip checking if connectors aren't enabled.
    MaybeScanForDisallowedFiles(idx + 1);
    return;
  }

  progress_->state = State::kScanning;
  progress_->sources_scanned = idx + 1;
  progress_callback_.Run(*progress_);

  DCHECK_EQ(file_transfer_analysis_delegates_.size(),
            progress_->sources.size());

  file_transfer_analysis_delegates_[idx] =
      enterprise_connectors::FileTransferAnalysisDelegate::Create(
          safe_browsing::DeepScanAccessPoint::FILE_TRANSFER,
          progress_->sources[idx].url, progress_->GetDestinationFolder(),
          profile_, file_system_context_.get(),
          std::move(settings_[idx].value()));

  file_transfer_analysis_delegates_[idx]->UploadData(
      base::BindOnce(&CopyOrMoveIOTaskPolicyImpl::MaybeScanForDisallowedFiles,
                     weak_ptr_factory_.GetWeakPtr(), idx + 1));
}

void CopyOrMoveIOTaskPolicyImpl::ScanningCompleted() {
  if (!MaybeShowConnectorsWarning()) {
    // Only start the transfer if no warning was shown.
    // If a warning is shown, the transfer will be resumed or aborted through
    // the warning dialog/toasts/etc.
    StartTransfer();
  }
}

bool CopyOrMoveIOTaskPolicyImpl::MaybeShowConnectorsWarning() {
  bool connectors_new_ui_enabled = base::FeatureList::IsEnabled(
      features::kFileTransferEnterpriseConnectorUI);
  if (!connectors_new_ui_enabled) {
    return false;
  }

  std::vector<base::FilePath> warning_files_paths;
  for (const auto& delegate : file_transfer_analysis_delegates_) {
    if (!delegate) {
      continue;
    }
    for (const auto& warned_file : delegate->GetWarnedFiles()) {
      warning_files_paths.push_back(warned_file.path());
    }
  }

  if (warning_files_paths.empty()) {
    return false;
  }

  auto* fpnm =
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          profile_);
  if (!fpnm) {
    LOG(ERROR) << "No FilesPolicyNotificationManager instantiated,"
                  "can't show policy warning UI";
    return false;
  }

  fpnm->ShowConnectorsWarning(
      base::BindOnce(&CopyOrMoveIOTaskPolicyImpl::OnConnectorsWarnDialogResult,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(progress_->task_id), std::move(warning_files_paths),
      progress_->type == file_manager::io_task::OperationType::kMove
          ? policy::dlp::FileAction::kMove
          : policy::dlp::FileAction::kCopy);
  return true;
}

// TODO(b/293122562): The user justification should be passed to this callback
// when proceeding a warning.
void CopyOrMoveIOTaskPolicyImpl::OnConnectorsWarnDialogResult(
    bool should_proceed) {
  if (!should_proceed) {
    // No need to cancel. Cancel will be called from
    // FilesPolicyNotificationManager.
    return;
  }
  // If the user has proceeded the warning, then we need to notify the
  // `FileTransferAnalysisDelegate`s to report the bypass of the warning and to
  // mark warned files as allowed for a transfer.
  base::ranges::for_each(file_transfer_analysis_delegates_,
                         [](const auto& delegate) {
                           if (delegate) {
                             // TODO(b/293122562): Pass user_justification.
                             delegate->BypassWarnings(absl::nullopt);
                           }
                         });
  StartTransfer();
}

void CopyOrMoveIOTaskPolicyImpl::IsTransferAllowed(
    size_t idx,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    IsTransferAllowedCallback callback) {
  DCHECK(!report_only_scans_);
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

  if (base::FeatureList::IsEnabled(
          features::kFileTransferEnterpriseConnectorUI)) {
    blocked_files_++;
    connectors_blocked_files_.push_back(source_url.path());
    if (blocked_file_name_.empty()) {
      blocked_file_name_ = util::GetDisplayablePath(profile_, source_url.path())
                               .value_or(base::FilePath())
                               .BaseName()
                               .value();
    }
  }

  std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
}

void CopyOrMoveIOTaskPolicyImpl::OnCheckIfTransferAllowed(
    std::vector<storage::FileSystemURL> blocked_entries) {
  // This function won't be reached if the user cancelled the DLP warning or the
  // DLP warning timed out.
  // TODO(b/279029167): If there's any file blocked by DLP, skip Enterprise
  // Connectors scanning for them.

  if (!blocked_entries.empty()) {
    blocked_files_ = blocked_entries.size();
    blocked_file_name_ =
        util::GetDisplayablePath(profile_, *blocked_entries.begin())
            .value_or(base::FilePath())
            .BaseName()
            .value();
  }

  if (settings_.empty() || report_only_scans_) {
    // Re-enter state progress if needed.
    if (progress_->state != State::kInProgress) {
      progress_->state = State::kInProgress;
      progress_callback_.Run(*progress_);
    }
    // Don't do any scans. It's either dlp-only restrictions (if `settings_` is
    // empty), or the scans will performed after the copy/move is completed
    // (report_only_scans_ is true).
    StartTransfer();
    return;
  }

  // Allocate one unique_ptr for each source. If it is not set, scanning is not
  // enabled for this source.
  file_transfer_analysis_delegates_.resize(progress_->sources.size());
  MaybeScanForDisallowedFiles(0);
}

}  // namespace file_manager::io_task
