// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_io_task_policy_impl.h"

#include <memory>
#include <optional>
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
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog.h"
#include "chrome/browser/ash/policy/dlp/dialogs/files_policy_dialog_utils.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager.h"
#include "chrome/browser/ash/policy/dlp/files_policy_notification_manager_factory.h"
#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/common/chrome_features.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate_composite.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_url.h"

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
    std::vector<std::optional<enterprise_connectors::AnalysisSettings>>
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
    std::vector<std::optional<enterprise_connectors::AnalysisSettings>>
        settings,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    ProgressStatus status) {
  // If there was an out-of-space error in the transfer, not all outputs might
  // be populated as the transfer is aborted on out-of-space errors.
  // So we truncate the settings, sources and outputs to only the first
  // `num_good_files` entries.
  const size_t num_good_files =
      std::min({settings.size(), status.sources.size(), status.outputs.size()});

  std::vector<storage::FileSystemURL> sources(num_good_files);
  std::vector<storage::FileSystemURL> outputs(num_good_files);
  for (size_t i = 0; i < num_good_files; ++i) {
    sources[i] = status.sources[i].url;
    outputs[i] = status.outputs[i].url;
  }
  settings.resize(num_good_files);

  // Notify the Files app of completion of the copy/move.
  std::move(io_task_completion_callback).Run(std::move(status));

  // Start the actual scanning.
  DoReportOnlyScanning(nullptr, 0, std::move(settings), std::move(sources),
                       std::move(outputs), profile, file_system_context);
}

// Notify FilesPolicyNotificationManager of files that were blocked by
// enterprise connectors to show proper blocked dialog.
// This is not done if the new UI for enterprise connectors is disabled.
void MaybeSendConnectorsBlockedFilesNotification(
    Profile* profile,
    std::map<policy::FilesPolicyDialog::BlockReason,
             policy::FilesPolicyDialog::Info> dialog_info_map,
    IOTaskId task_id,
    OperationType type) {
  if (dialog_info_map.empty()) {
    return;
  }

  // Blocked files are only added if kFileTransferEnterpriseConnectorUI is
  // enabled.
  CHECK(base::FeatureList::IsEnabled(
      features::kFileTransferEnterpriseConnectorUI));

  auto* files_policy_manager =
      policy::FilesPolicyNotificationManagerFactory::GetForBrowserContext(
          profile);
  if (!files_policy_manager) {
    LOG(ERROR) << "Couldn't find FilesPolicyNotificationManager";
    return;
  }

  for (const auto& [block_reason, dialog_info] : dialog_info_map) {
    files_policy_manager->SetConnectorsBlockedFiles(
        task_id,
        type == file_manager::io_task::OperationType::kMove
            ? policy::dlp::FileAction::kMove
            : policy::dlp::FileAction::kCopy,
        block_reason, std::move(dialog_info));
  }
}

}  // namespace

CopyOrMoveIOTaskPolicyImpl::CopyOrMoveIOTaskPolicyImpl(
    OperationType type,
    ProgressStatus& progress,
    std::vector<base::FilePath> destination_file_names,
    std::vector<std::optional<enterprise_connectors::AnalysisSettings>>
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
        [](const std::optional<enterprise_connectors::AnalysisSettings>&
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

void CopyOrMoveIOTaskPolicyImpl::Complete(State state) {
  bool has_dlp_errors = !dlp_blocked_files_.empty();
  bool has_connector_errors = !connectors_blocked_files_.empty();
  if ((has_dlp_errors || has_connector_errors) &&
      base::FeatureList::IsEnabled(features::kNewFilesPolicyUX)) {
    // TODO(b/293425493): Support combined error type (if both dlp and connector
    // errors exist).
    PolicyErrorType error_type = has_dlp_errors
                                     ? PolicyErrorType::kDlp
                                     : PolicyErrorType::kEnterpriseConnectors;
    // Used for notifications.
    base::FilePath blocked_file_path =
        has_dlp_errors ? (*dlp_blocked_files_.begin())
                       : connectors_blocked_files_.begin()->second[0];
    std::string blocked_file_name =
        util::GetDisplayablePath(profile_, blocked_file_path)
            .value_or(base::FilePath())
            .BaseName()
            .value();
    bool always_show_review = false;

    std::map<policy::FilesPolicyDialog::BlockReason,
             policy::FilesPolicyDialog::Info>
        dialog_info_map;
    for (const auto& [reason, paths] : connectors_blocked_files_) {
      if (paths.empty()) {
        continue;
      }
      auto dialog_info = policy::files_dialog_utils::
          GetDialogInfoForEnterpriseConnectorsBlockReason(
              reason, paths, file_transfer_analysis_delegates_);
      always_show_review |= dialog_info.HasCustomDetails();
      dialog_info_map.insert({reason, std::move(dialog_info)});
    }

    progress_->policy_error.emplace(
        error_type,
        (dlp_blocked_files_.size() + GetConnectorsBlockedFilesNum()),
        blocked_file_name, always_show_review);
    state = State::kError;

    MaybeSendConnectorsBlockedFilesNotification(
        profile_, dialog_info_map, progress_->task_id, progress_->type);
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
        progress_->type == file_manager::io_task::OperationType::kMove;
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
  if (report_only_scans_ && dlp_blocked_files_.empty()) {
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
  auto defaultHook = CopyOrMoveIOTaskImpl::GetHookDelegate(idx);

  if (settings_.empty() || report_only_scans_) {
    // For DLP only restrictions or report-only scans, no blocking should be
    // performed, so we use the normal delegate.
    return defaultHook;
  }

  DCHECK_LT(idx, file_transfer_analysis_delegates_.size());
  if (!file_transfer_analysis_delegates_[idx]) {
    // If scanning is disabled, use the normal delegate.
    // Scanning can be disabled if some source_urls lie on a file system for
    // which scanning is enabled, while other source_urls lie on a file system
    // for which scanning is disabled.
    return defaultHook;
  }

  // For all callbacks, we are using CreateRelayCallback to ensure that the
  // callbacks are executed on the current (i.e., UI) thread.
  auto file_check_callback = google_apis::CreateRelayCallback(
      base::BindRepeating(&CopyOrMoveIOTaskPolicyImpl::IsTransferAllowed,
                          weak_ptr_factory_.GetWeakPtr(), idx));
  auto checkHook = std::make_unique<FileManagerCopyOrMoveHookFileCheckDelegate>(
      file_system_context_, file_check_callback);
  return storage::CopyOrMoveHookDelegateComposite::CreateOrAdd(
      std::move(defaultHook), std::move(checkHook));
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
  // TODO(b/315783871): recursively count the files in directories and pass this
  // value to the js side to show the proper singular/plural scanning label.
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

  // Currently, the custom warning message, the custom learn more URL, and the
  // bypass justification flag values are only relevant for warning scenarios.
  // Moreover, these values are consistent across all valid
  // `file_transfer_analysis_delegates_` having warned results, so we just
  // retrieve these values from the first of such delegates. There are as many
  // delegates as the number of sources. A delegate in
  // `file_transfer_analysis_delegates_` is valid if for the
  // source-destination-pair scanning is enabled, nullptr otherwise.
  auto delegate_it = base::ranges::find_if(
      file_transfer_analysis_delegates_,
      [](const std::unique_ptr<
          enterprise_connectors::FileTransferAnalysisDelegate>& delegate) {
        return delegate != nullptr;
      });

  // Warning mode is only available for the "dlp" tag (sensitive data), since
  // "malware" results are always blocked.
  auto dialog_info = policy::FilesPolicyDialog::Info::Warn(
      policy::FilesPolicyDialog::BlockReason::
          kEnterpriseConnectorsSensitiveData,
      warning_files_paths);
  if (delegate_it != file_transfer_analysis_delegates_.end()) {
    auto* valid_delegate = delegate_it->get();
    dialog_info.SetBypassRequiresJustification(
        valid_delegate->BypassRequiresJustification(
            enterprise_connectors::kDlpTag));
    dialog_info.SetMessage(
        valid_delegate->GetCustomMessage(enterprise_connectors::kDlpTag));
    dialog_info.SetLearnMoreURL(
        valid_delegate->GetCustomLearnMoreUrl(enterprise_connectors::kDlpTag));
  }

  fpnm->ShowConnectorsWarning(
      base::BindOnce(&CopyOrMoveIOTaskPolicyImpl::OnConnectorsWarnDialogResult,
                     weak_ptr_factory_.GetWeakPtr()),
      std::move(progress_->task_id),
      progress_->type == file_manager::io_task::OperationType::kMove
          ? policy::dlp::FileAction::kMove
          : policy::dlp::FileAction::kCopy,
      std::move(dialog_info));
  return true;
}

void CopyOrMoveIOTaskPolicyImpl::OnConnectorsWarnDialogResult(
    std::optional<std::u16string> user_justification,
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
                         [&user_justification](const auto& delegate) {
                           if (delegate) {
                             delegate->BypassWarnings(user_justification);
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
  // If a file is blocked by DLP, skip Enterprise Connectors scanning for it
  // since Enterprise Connectors won't be able to scan it anyway. The file be
  // blocked by the DLP daemon later.
  if (result.IsAllowed() ||
      base::Contains(dlp_blocked_files_, source_url.path())) {
    std::move(callback).Run(base::File::FILE_OK);
    return;
  }
  DCHECK(result.IsUnknown() || result.IsBlocked());

  if (base::FeatureList::IsEnabled(
          features::kFileTransferEnterpriseConnectorUI)) {
    auto& paths = connectors_blocked_files_
        [policy::files_dialog_utils::GetEnterpriseConnectorsBlockReason(
            result)];
    paths.push_back(source_url.path());
  }

  std::move(callback).Run(base::File::FILE_ERROR_SECURITY);
}

void CopyOrMoveIOTaskPolicyImpl::OnCheckIfTransferAllowed(
    std::vector<storage::FileSystemURL> blocked_entries) {
  // Check if the task was cancelled by the user.
  if (progress_->state == State::kCancelled) {
    return;
  }

  for (const auto& entry : blocked_entries) {
    dlp_blocked_files_.insert(entry.path());
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

size_t CopyOrMoveIOTaskPolicyImpl::GetConnectorsBlockedFilesNum() const {
  size_t num = 0;
  for (const auto& [_, paths] : connectors_blocked_files_) {
    num += paths.size();
  }
  return num;
}

}  // namespace file_manager::io_task
