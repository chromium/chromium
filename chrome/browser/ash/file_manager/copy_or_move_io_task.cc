// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_io_task.h"

#include <cmath>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/check_op.h"
#include "base/files/file_path.h"
#include "base/functional/callback.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_impl.h"
#include "chrome/browser/ash/file_manager/copy_or_move_io_task_policy_impl.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/policy/dlp/dlp_files_controller_ash.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_rules_manager_factory.h"
#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"
#include "chrome/common/chrome_features.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_url.h"

namespace file_manager::io_task {

CopyOrMoveIOTask::CopyOrMoveIOTask(
    OperationType type,
    std::vector<storage::FileSystemURL> source_urls,
    storage::FileSystemURL destination_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    bool show_notification)
    : IOTask(show_notification),
      profile_(profile),
      file_system_context_(file_system_context) {
  DCHECK(type == OperationType::kCopy || type == OperationType::kMove);
  progress_.state = State::kQueued;
  progress_.type = type;
  progress_.SetDestinationFolder(std::move(destination_folder), profile);
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;

  for (const auto& url : source_urls) {
    progress_.sources.emplace_back(url, std::nullopt);
  }

  source_urls_ = std::move(source_urls);
}

CopyOrMoveIOTask::CopyOrMoveIOTask(
    OperationType type,
    std::vector<storage::FileSystemURL> source_urls,
    std::vector<base::FilePath> destination_file_names,
    storage::FileSystemURL destination_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    bool show_notification)
    : CopyOrMoveIOTask(type,
                       source_urls,
                       std::move(destination_folder),
                       profile,
                       file_system_context,
                       show_notification) {
  DCHECK_EQ(source_urls.size(), destination_file_names.size());
  destination_file_names_ = std::move(destination_file_names);
}

CopyOrMoveIOTask::~CopyOrMoveIOTask() = default;

void CopyOrMoveIOTask::Execute(IOTask::ProgressCallback progress_callback,
                               IOTask::CompleteCallback complete_callback) {
  // Check if DLP files restrictions are enabled.
  bool dlp_files_enabled =
      !!policy::DlpFilesControllerAsh::GetForPrimaryProfile();

  // Check if scanning is enabled.
  bool scanning_feature_enabled =
      base::FeatureList::IsEnabled(features::kFileTransferEnterpriseConnector);
  std::vector<std::optional<enterprise_connectors::AnalysisSettings>>
      scanning_settings;
  if (scanning_feature_enabled) {
    scanning_settings =
        enterprise_connectors::FileTransferAnalysisDelegate::IsEnabledVec(
            profile_, source_urls_, progress_.GetDestinationFolder());
  }

  if (dlp_files_enabled || !scanning_settings.empty()) {
    impl_ = std::make_unique<CopyOrMoveIOTaskPolicyImpl>(
        progress_.type, progress_, std::move(destination_file_names_),
        std::move(scanning_settings), progress_.GetDestinationFolder(),
        profile_, file_system_context_, progress_.show_notification);
  } else {
    impl_ = std::make_unique<CopyOrMoveIOTaskImpl>(
        progress_.type, progress_, std::move(destination_file_names_),
        progress_.GetDestinationFolder(), profile_, file_system_context_,
        progress_.show_notification);
  }

  impl_->Execute(std::move(progress_callback), std::move(complete_callback));
}

void CopyOrMoveIOTask::Pause(PauseParams params) {
  if (impl_) {
    impl_->Pause(std::move(params));
  }
}

void CopyOrMoveIOTask::Resume(ResumeParams params) {
  if (impl_) {
    impl_->Resume(std::move(params));
  }
}

void CopyOrMoveIOTask::Cancel() {
  progress_.state = State::kCancelled;
  if (impl_) {
    impl_->Cancel();
  }
}

void CopyOrMoveIOTask::CompleteWithError(PolicyError policy_error) {
  if (impl_) {
    impl_->CompleteWithError(std::move(policy_error));
  }
}

}  // namespace file_manager::io_task
