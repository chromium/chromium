// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/copy_or_move_io_task_impl.h"

#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "ash/constants/ash_features.h"
#include "base/check_op.h"
#include "base/files/file.h"
#include "base/files/file_error_or.h"
#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/notreached.h"
#include "base/system/sys_info.h"
#include "base/task/bind_post_task.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/time/time.h"
#include "chrome/browser/ash/drive/drive_integration_service.h"
#include "chrome/browser/ash/drive/file_system_util.h"
#include "chrome/browser/ash/file_manager/copy_or_move_encrypted_hook_delegate.h"
#include "chrome/browser/ash/file_manager/file_manager_copy_or_move_hook_delegate.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/io_task.h"
#include "chrome/browser/ash/file_manager/io_task_util.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/ui/webui/ash/cloud_upload/cloud_upload_dialog.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "google_apis/common/task_util.h"
#include "storage/browser/file_system/copy_or_move_hook_delegate_composite.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "third_party/cros_system_api/constants/cryptohome.h"

namespace file_manager::io_task {
namespace {

std::string Redact(const storage::FileSystemURL& url) {
  return LOG_IS_ON(INFO) ? url.DebugString() : "(redacted)";
}

bool* DestinationNoSpace() {
  static bool destination_no_space = false;
  return &destination_no_space;
}

// Starts the copy operation via FileSystemOperationRunner.
storage::FileSystemOperationRunner::OperationID StartCopyOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    storage::FileSystemOperation::CopyOrMoveOptionSet options,
    storage::FileSystemOperation::ErrorBehavior error_behavior,
    std::unique_ptr<storage::CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
    storage::FileSystemOperation::StatusCallback complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->Copy(
      source_url, destination_url, options, error_behavior,
      std::move(copy_or_move_hook_delegate), std::move(complete_callback));
}

// Starts the move operation via FileSystemOperationRunner.
storage::FileSystemOperationRunner::OperationID StartMoveOnIOThread(
    scoped_refptr<storage::FileSystemContext> file_system_context,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    storage::FileSystemOperation::CopyOrMoveOptionSet options,
    storage::FileSystemOperation::ErrorBehavior error_behavior,
    std::unique_ptr<storage::CopyOrMoveHookDelegate> copy_or_move_hook_delegate,
    storage::FileSystemOperation::StatusCallback complete_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  return file_system_context->operation_runner()->Move(
      source_url, destination_url, options, error_behavior,
      std::move(copy_or_move_hook_delegate), std::move(complete_callback));
}

// Helper function for copy or move tasks that determines whether or not entries
// identified by their URLs should be considered as being on the different file
// systems or not.
bool IsCrossFileSystem(Profile* const profile,
                       const storage::FileSystemURL& source_url,
                       const storage::FileSystemURL& destination_url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile);

  const base::WeakPtr<const file_manager::Volume> source_volume =
      volume_manager->FindVolumeFromPath(source_url.path());
  const base::WeakPtr<const file_manager::Volume> destination_volume =
      volume_manager->FindVolumeFromPath(destination_url.path());

  // When either volume is unavailable, fall back to only checking the
  // filesystem ID, which uniquely maps a URL to its ExternalMountPoints
  // instance. NOTE 1: different volumes (e.g. for removables) might share the
  // same ExternalMountPoints. NOTE 2: if either volume is unavailable, the
  // operation itself is likely to fail.
  if (!source_volume || !destination_volume) {
    return source_url.filesystem_id() != destination_url.filesystem_id();
  }

  VLOG(1) << "IsCrossFileSystem: " << source_volume->volume_id() << " -> "
          << destination_volume->volume_id();

  return source_volume->volume_id() != destination_volume->volume_id();
}

}  // namespace

ItemProgress::ItemProgress() = default;
ItemProgress::~ItemProgress() = default;

CopyOrMoveIOTaskImpl::CopyOrMoveIOTaskImpl(
    OperationType type,
    ProgressStatus& progress,
    std::vector<base::FilePath> destination_file_names,
    storage::FileSystemURL destination_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    bool show_notification)
    : progress_(progress),
      profile_(profile),
      file_system_context_(file_system_context),
      source_sizes_(progress_->sources.size()),
      item_progresses(progress_->sources.size()) {
  DCHECK(type == OperationType::kCopy || type == OperationType::kMove);
  if (!destination_file_names.empty()) {
    DCHECK_EQ(progress_->sources.size(), destination_file_names.size());
  }
  destination_file_names_ = std::move(destination_file_names);
}

CopyOrMoveIOTaskImpl::~CopyOrMoveIOTaskImpl() {
  if (operation_id_) {
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](scoped_refptr<storage::FileSystemContext> file_system_context,
               storage::FileSystemOperationRunner::OperationID operation_id) {
              file_system_context->operation_runner()->Cancel(
                  operation_id, base::DoNothing());
            },
            file_system_context_, *operation_id_));
  }
}

// static
bool CopyOrMoveIOTaskImpl::IsCrossFileSystemForTesting(
    Profile* profile,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url) {
  return IsCrossFileSystem(profile, source_url, destination_url);
}

// static
void CopyOrMoveIOTaskImpl::SetDestinationNoSpaceForTesting(
    bool destination_no_space) {
  *DestinationNoSpace() = destination_no_space;
}

void CopyOrMoveIOTaskImpl::Execute(IOTask::ProgressCallback progress_callback,
                                   IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  if (progress_->sources.size() == 0) {
    Complete(State::kSuccess);
    return;
  }

  VerifyTransfer();
}

void CopyOrMoveIOTaskImpl::Pause(PauseParams params) {
  progress_->state = State::kPaused;
  progress_->pause_params = params;
  progress_callback_.Run(*progress_);
}

void CopyOrMoveIOTaskImpl::Resume(ResumeParams params) {
  if (params.policy_params.has_value()) {
    LOG(ERROR)
        << "Policy resume should be handled by CopyOrMoveIOTaskPolicyImpl";
    Complete(State::kError);
    return;
  }
  if (!params.conflict_params.has_value()) {
    LOG(ERROR) << "Missing resume conflict params";
    Complete(State::kError);
  }

  LOG_IF(ERROR, !resume_callback_) << "Resume but no resume_callback_";

  if (resume_callback_) {
    std::move(resume_callback_).Run(std::move(params));
  }
}

void CopyOrMoveIOTaskImpl::Cancel() {
  progress_->state = State::kCancelled;
  // Any in-flight operation will be cancelled when the task is destroyed.
}

// Calls the completion callback for the task. |progress_| should not be
// accessed after calling this.
void CopyOrMoveIOTaskImpl::Complete(State state) {
  completed_ = true;
  progress_->state = state;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(*progress_)));
}

void CopyOrMoveIOTaskImpl::CompleteWithError(PolicyError policy_error) {
  progress_->state = State::kError;
  progress_->policy_error.emplace(std::move(policy_error));
}

void CopyOrMoveIOTaskImpl::VerifyTransfer() {
  // TODO(b/280947989) remove this code once Multi-user sign-in is deprecated.
  // Prevent files being copied or moved to ODFS if there is a managed user
  // present amongst other logged in users. Ensures managed user's files can't
  // be leaked to a non-managed user's ODFS b/278644796.
  if (ash::cloud_upload::UrlIsOnODFS(progress_->GetDestinationFolder()) &&
      user_manager::UserManager::Get()->GetLoggedInUsers().size() > 1) {
    // Check none of the logged in users are managed.
    for (user_manager::User* user :
         user_manager::UserManager::Get()->GetLoggedInUsers()) {
      Profile* user_profile = Profile::FromBrowserContext(
          ash::BrowserContextHelper::Get()->GetBrowserContextByUser(user));
      if (user_profile->GetProfilePolicyConnector()->IsManaged()) {
        Complete(State::kError);
        return;
      }
    }
  }

  StartTransfer();
}

void CopyOrMoveIOTaskImpl::StartTransfer() {
  progress_->state = State::kInProgress;

  // Start the transfer by getting the file size.
  for (size_t i = 0; i < progress_->sources.size(); i++) {
    GetFileSize(i);
  }
}

// Computes the total size of all source files and stores it in
// |progress_.total_bytes|.
void CopyOrMoveIOTaskImpl::GetFileSize(size_t idx) {
  DCHECK(idx < progress_->sources.size());

  const base::FilePath& source = progress_->sources[idx].url.path();
  const base::FilePath& destination = progress_->GetDestinationFolder().path();

  constexpr storage::FileSystemOperation::GetMetadataFieldSet metadata_fields =
      {storage::FileSystemOperation::GetMetadataField::kIsDirectory,
       storage::FileSystemOperation::GetMetadataField::kSize,
       storage::FileSystemOperation::GetMetadataField::kRecursiveSize};

  auto get_metadata_callback =
      base::BindOnce(&GetFileMetadataOnIOThread, file_system_context_,
                     progress_->sources[idx].url, metadata_fields,
                     google_apis::CreateRelayCallback(
                         base::BindOnce(&CopyOrMoveIOTaskImpl::GotFileSize,
                                        weak_ptr_factory_.GetWeakPtr(), idx)));

  if (file_manager::util::IsDriveLocalPath(profile_, source) &&
      file_manager::file_tasks::IsOfficeFile(source) &&
      !file_manager::util::IsDriveLocalPath(profile_, destination)) {
    if (progress_->type == OperationType::kCopy) {
      UMA_HISTOGRAM_ENUMERATION(
          file_manager::file_tasks::kUseOutsideDriveMetricName,
          file_manager::file_tasks::OfficeFilesUseOutsideDriveHook::COPY);
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          file_manager::file_tasks::kUseOutsideDriveMetricName,
          file_manager::file_tasks::OfficeFilesUseOutsideDriveHook::MOVE);
    }
    auto* drive_service = drive::util::GetIntegrationServiceByProfile(profile_);
    if (drive_service) {
      drive_service->ForceReSyncFile(
          source,
          base::BindPostTask(content::GetIOThreadTaskRunner({}),
                             std::move(get_metadata_callback), FROM_HERE));
      return;
    }
    // If there is no Drive connection, we should continue as normal.
  }

  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE, std::move(get_metadata_callback));
}

// Helper function to GetFileSize() that is called when the metadata for a file
// is retrieved.
void CopyOrMoveIOTaskImpl::GotFileSize(size_t idx,
                                       base::File::Error error,
                                       const base::File::Info& file_info) {
  if (completed_) {
    // If Complete() has been called (e.g. due to an error), |progress_| is no
    // longer valid, so return immediately.
    return;
  }

  DCHECK(idx < progress_->sources.size());
  if (error != base::File::FILE_OK) {
    progress_->sources[idx].error = error;
    LOG(ERROR) << "Could not get size of source file: error " << error << " "
               << base::File::ErrorToString(error);
    Complete(State::kError);
    return;
  }

  progress_->total_bytes += file_info.size;
  source_sizes_[idx] = file_info.size;
  progress_->sources[idx].is_directory = file_info.is_directory;

  // Return early if we didn't yet get the file size for all files.
  DCHECK_LT(files_preprocessed_, progress_->sources.size());
  if (++files_preprocessed_ < progress_->sources.size()) {
    return;
  }

  // Got file size for all files at this point!
  speedometer_.SetTotalBytes(progress_->total_bytes);

  if (!progress_->GetDestinationFolder().TypeImpliesPathIsReal()) {
    // Destination is a virtual filesystem, so skip checking free space.
    GenerateDestinationURL(0);
  } else {
    // For Drive, check we have enough local disk first, then check quota.
    base::FilePath path = progress_->GetDestinationFolder().path();
    auto* drive_integration_service =
        drive::util::GetIntegrationServiceByProfile(profile_);
    if (drive_integration_service && drive_integration_service->IsMounted() &&
        drive_integration_service->GetMountPointPath().IsParent(
            progress_->GetDestinationFolder().path())) {
      path = drive_integration_service->GetDriveFsHost()->GetDataPath();
    }
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace, path),
        base::BindOnce(&CopyOrMoveIOTaskImpl::GotFreeDiskSpace,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

// Ensures that there is enough free space on the destination volume.
void CopyOrMoveIOTaskImpl::GotFreeDiskSpace(int64_t free_space) {
  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  bool is_drive = drive_integration_service &&
                  drive_integration_service->IsMounted() &&
                  drive_integration_service->GetMountPointPath().IsParent(
                      progress_->GetDestinationFolder().path());
  if (progress_->GetDestinationFolder().filesystem_id() ==
          util::GetDownloadsMountPointName(profile_) ||
      is_drive) {
    free_space -= cryptohome::kMinFreeSpaceInBytes;
  }

  int64_t required_bytes = progress_->total_bytes;

  // Move operations that are same-filesystem do not require disk space.
  if (progress_->type == OperationType::kMove) {
    for (size_t i = 0; i < source_sizes_.size(); i++) {
      if (!IsCrossFileSystem(profile_, progress_->sources[i].url,
                             progress_->GetDestinationFolder())) {
        required_bytes -= source_sizes_[i];
      }
    }
  }

  if (required_bytes > free_space || *DestinationNoSpace()) {
    progress_->outputs.emplace_back(progress_->GetDestinationFolder(),
                                    base::File::FILE_ERROR_NO_SPACE);
    LOG(ERROR) << "Insufficient free space in destination";
    Complete(State::kError);
    return;
  }

  if (is_drive) {
    bool is_shared_drive = drive_integration_service->IsSharedDrive(
        progress_->GetDestinationFolder().path());
    drive_integration_service->GetPooledQuotaUsage(
        base::BindOnce(base::BindOnce(
            &CopyOrMoveIOTaskImpl::GotDrivePooledQuota,
            weak_ptr_factory_.GetWeakPtr(), required_bytes, is_shared_drive)));
    return;
  }

  GenerateDestinationURL(0);
}

void CopyOrMoveIOTaskImpl::GotDrivePooledQuota(
    int64_t required_bytes,
    bool is_shared_drive,
    drive::FileError error,
    drivefs::mojom::PooledQuotaUsagePtr usage) {
  if (error != drive::FileError::FILE_ERROR_OK) {
    // Log the error if we couldn't fetch the quota (probably because we are
    // offline), but continue the operation and we will show an error later
    // when we come back online and try to sync.
    LOG(ERROR) << "Error fetching drive quota: " << error;
  } else {
    bool org_exceeded =
        usage->user_type == drivefs::mojom::UserType::kOrganization &&
        usage->organization_limit_exceeded;
    // User quota does not apply to shared drives.
    bool user_exceeded =
        !is_shared_drive && usage->total_user_bytes != -1 &&
        (usage->total_user_bytes - usage->used_user_bytes) < required_bytes;
    if (org_exceeded || user_exceeded) {
      progress_->outputs.emplace_back(progress_->GetDestinationFolder(),
                                      base::File::FILE_ERROR_NO_SPACE);
      LOG(ERROR) << "Insufficient drive quota";
      Complete(State::kError);
      return;
    }
  }

  // Check shared drive quota if applicable.
  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  if (is_shared_drive && drive_integration_service &&
      drive_integration_service->IsMounted()) {
    drive_integration_service->GetMetadata(
        progress_->GetDestinationFolder().path(),
        base::BindOnce(&CopyOrMoveIOTaskImpl::GotSharedDriveMetadata,
                       weak_ptr_factory_.GetWeakPtr(), required_bytes));
    return;
  }

  GenerateDestinationURL(0);
}

void CopyOrMoveIOTaskImpl::GotSharedDriveMetadata(
    int64_t required_bytes,
    drive::FileError error,
    drivefs::mojom::FileMetadataPtr metadata) {
  if (error != drive::FileError::FILE_ERROR_OK) {
    // Log the error if we couldn't fetch the metadata (probably because we are
    // offline), but continue the operation and we will show an error later
    // when we come back online and try to sync.
    LOG(ERROR) << "Error fetching shared drive metadata: " << error;
  } else if (metadata->shared_drive_quota) {
    const auto& quota = metadata->shared_drive_quota;
    if ((quota->individual_quota_bytes_total -
         quota->quota_bytes_used_in_drive) < required_bytes) {
      progress_->outputs.emplace_back(progress_->GetDestinationFolder(),
                                      base::File::FILE_ERROR_NO_SPACE);
      LOG(ERROR) << "Insufficient shared drive quota";
      Complete(State::kError);
      return;
    }
  }

  GenerateDestinationURL(0);
}

// Tries to find an unused filename in the destination folder for a specific
// entry being transferred.
void CopyOrMoveIOTaskImpl::GenerateDestinationURL(size_t idx) {
  DCHECK(idx < progress_->sources.size());

  // In the event no `destination_file_names_` exist, fall back to the
  // `BaseName` from the source URL.
  const auto destination_file_name =
      (destination_file_names_.size() == progress_->sources.size())
          ? destination_file_names_[idx]
          : progress_->sources[idx].url.path().BaseName();

  util::GenerateUnusedFilename(
      progress_->GetDestinationFolder(), destination_file_name,
      file_system_context_,
      base::BindOnce(&CopyOrMoveIOTaskImpl::CopyOrMoveFile,
                     weak_ptr_factory_.GetWeakPtr(), idx));
}

// Starts the underlying copy or move operation.
void CopyOrMoveIOTaskImpl::CopyOrMoveFile(
    size_t idx,
    base::FileErrorOr<storage::FileSystemURL> destination_result) {
  DCHECK(idx < progress_->sources.size());

  if (!destination_result.has_value()) {
    progress_->outputs.emplace_back(progress_->GetDestinationFolder(),
                                    std::nullopt);
    OnCopyOrMoveComplete(idx, destination_result.error());
    return;
  }

  progress_->outputs.emplace_back(destination_result.value(), std::nullopt);
  DCHECK_EQ(idx + 1, progress_->outputs.size());

  const storage::FileSystemURL& source_url = progress_->sources[idx].url;
  const storage::FileSystemURL& destination_url = destination_result.value();

  // If the conflict dialog feature is disabled, use the destination url to
  // implement default files app behavior: 'keepboth'.
  if (!ash::features::IsFilesConflictDialogEnabled()) {
    ContinueCopyOrMoveFile(idx, std::move(destination_url));
    return;
  }

  // Create a replace url using the source base name and destination folder
  // as the parent directory.
  auto basename = source_url.path().BaseName();
  auto replace_url = file_system_context_->CreateCrackedFileSystemURL(
      progress_->GetDestinationFolder().storage_key(),
      progress_->GetDestinationFolder().mount_type(),
      progress_->GetDestinationFolder().virtual_path().Append(
          base::FilePath::FromUTF8Unsafe(basename.AsUTF8Unsafe())));

  // If the source url and replace url are the same, the copy/move operation
  // must use the destination url: default files app behavior 'keepboth'.
  if (source_url == replace_url) {
    ContinueCopyOrMoveFile(idx, std::move(destination_url));
    return;
  }

  // Otherwise, if the base names are the same, there is no conflict and the
  // copy/move operation can use the destination url.
  if (basename == destination_url.path().BaseName()) {
    ContinueCopyOrMoveFile(idx, std::move(destination_url));
    return;
  }

  // If the base names are not the same, then the destination url exists and
  // we must resolve the file name conflict.  If the user's previous resolve
  // was 'ApplyToAll', |conflict_resolve_| contains 'keepboth' or 'replace'.
  // Use it to automatically resolve the conflict (no need to ask the UI).
  if (!conflict_resolve_.empty()) {
    ResumeParams params;
    params.conflict_params.emplace();
    params.conflict_params->conflict_resolve = conflict_resolve_;
    params.conflict_params->conflict_apply_to_all = true;
    ResumeCopyOrMoveFile(idx, std::move(replace_url),
                         std::move(destination_url), std::move(params));
    return;
  }

  // Setup the resume callback prior to entering state::PAUSED. ResumeIOTask
  // will invoke this callback, once the user has resolved the conflict. See
  // CopyOrMoveIOTaskImpl::Resume().
  DCHECK(!resume_callback_);
  resume_callback_ = google_apis::CreateRelayCallback(
      base::BindOnce(&CopyOrMoveIOTaskImpl::ResumeCopyOrMoveFile,
                     weak_ptr_factory_.GetWeakPtr(), idx,
                     std::move(replace_url), std::move(destination_url)));

  // Enter state PAUSED: send pause params to the UI, to ask the user how to
  // resolve the file name conflict.
  auto destination_folder = file_system_context_->CreateCrackedFileSystemURL(
      progress_->GetDestinationFolder().storage_key(),
      progress_->GetDestinationFolder().mount_type(),
      progress_->GetDestinationFolder().virtual_path());
  progress_->state = State::kPaused;
  progress_->pause_params.conflict_params.emplace();
  progress_->pause_params.conflict_params->conflict_name =
      basename.AsUTF8Unsafe();
  progress_->pause_params.conflict_params->conflict_is_directory =
      progress_->sources[idx].is_directory;
  progress_->pause_params.conflict_params->conflict_multiple =
      (idx < progress_->sources.size() - 1);
  progress_->pause_params.conflict_params->conflict_target_url =
      destination_folder.ToGURL().spec();
  progress_callback_.Run(*progress_);
}

void CopyOrMoveIOTaskImpl::ResumeCopyOrMoveFile(
    size_t idx,
    storage::FileSystemURL replace_url,
    storage::FileSystemURL destination_url,
    ResumeParams params) {
  DCHECK(idx < progress_->sources.size());
  DCHECK(idx < progress_->outputs.size());

  // Re-enter state progress if needed.
  if (progress_->state != State::kInProgress) {
    progress_->state = State::kInProgress;
    progress_callback_.Run(*progress_);
  }

  // Get the user's conflict resolve choice.
  const std::string& conflict_resolve =
      params.conflict_params->conflict_resolve;
  const bool resolve_keepboth = conflict_resolve == "keepboth";
  const bool resolve_replace = conflict_resolve == "replace";

  // The Files app UI always returns valid conflict resolve values.
  if (!resolve_keepboth && !resolve_replace) {
    LOG(ERROR) << "Invalid conflict resolve: " << conflict_resolve;
    OnCopyOrMoveComplete(idx, base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  // Remember the 'ApplyToAll' choice for future conflict handling.
  if (conflict_resolve_.empty() &&
      params.conflict_params->conflict_apply_to_all) {
    conflict_resolve_ = conflict_resolve;
  }

  // For 'keepboth' resolve, use the destination url as the target.
  if (resolve_keepboth) {
    ContinueCopyOrMoveFile(idx, destination_url);
    return;
  }

  // For 'replace': delete replace_url so it can become the target.
  auto did_delete_callback = google_apis::CreateRelayCallback(
      base::BindOnce(&CopyOrMoveIOTaskImpl::DidDeleteDestinationURL,
                     weak_ptr_factory_.GetWeakPtr(), idx, replace_url));

  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&StartDeleteOnIOThread, file_system_context_, replace_url,
                     std::move(did_delete_callback)),
      base::BindOnce(&CopyOrMoveIOTaskImpl::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

void CopyOrMoveIOTaskImpl::DidDeleteDestinationURL(
    size_t idx,
    storage::FileSystemURL replace_url,
    base::File::Error error) {
  DCHECK(idx < progress_->sources.size());
  DCHECK(idx < progress_->outputs.size());

  operation_id_.reset();

  // If replace_url delete failed, a copy/move of the source to that url will
  // also fail. Report the error to OnCopyOrMoveComplete. Otherwise, call the
  // ContinueCopyOrMoveFile() flow with the replace url as the target.
  if (error) {
    OnCopyOrMoveComplete(idx, error);
  } else {
    ContinueCopyOrMoveFile(idx, replace_url);
  }
}

void CopyOrMoveIOTaskImpl::ContinueCopyOrMoveFile(
    size_t idx,
    storage::FileSystemURL destination_url) {
  DCHECK(idx < progress_->sources.size());
  DCHECK(idx < progress_->outputs.size());

  const storage::FileSystemURL& source_url = progress_->sources[idx].url;

  // For a source entry name 'test', the destination url base name will be:
  //  `test` if that entry name did not exist at the destination.
  //  `test` if that entry name existed at the destination and was deleted
  //         because the user choice was to 'replace' that entry.
  //  `test (2)` if the entry name exists at the destination and 'keepboth'
  //         is active, either by default behavior or by user choice.
  progress_->outputs[idx].url = destination_url;

  // File browsers generally default to preserving mtimes on copy/move so we
  // should do the same.
  storage::FileSystemOperation::CopyOrMoveOptionSet options = {
      storage::FileSystemOperation::CopyOrMoveOption::kPreserveLastModified,
      storage::FileSystemOperation::CopyOrMoveOption::
          kRemovePartiallyCopiedFilesOnError};

  // To ensure progress updates, force cross-filesystem I/O operations when the
  // source and the destination are on different volumes.
  if (IsCrossFileSystem(profile_, source_url, destination_url)) {
    options.Put(
        storage::FileSystemOperation::CopyOrMoveOption::kForceCrossFilesystem);
  }

  auto* transfer_function = progress_->type == OperationType::kCopy
                                ? &StartCopyOnIOThread
                                : &StartMoveOnIOThread;

  // Using CreateRelayCallback to ensure that the callbacks are executed on the
  // current thread.
  auto complete_callback = google_apis::CreateRelayCallback(
      base::BindOnce(&CopyOrMoveIOTaskImpl::OnCopyOrMoveComplete,
                     weak_ptr_factory_.GetWeakPtr(), idx));

  content::GetIOThreadTaskRunner({})->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(transfer_function, file_system_context_, source_url,
                     destination_url, options, GetErrorBehavior(),
                     GetHookDelegate(idx), std::move(complete_callback)),
      base::BindOnce(&CopyOrMoveIOTaskImpl::SetCurrentOperationID,
                     weak_ptr_factory_.GetWeakPtr()));
}

storage::FileSystemOperation::ErrorBehavior
CopyOrMoveIOTaskImpl::GetErrorBehavior() {
  return storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT;
}

bool CopyOrMoveIOTaskImpl::ShouldSkipEncryptedFiles() {
  if (!base::FeatureList::IsEnabled(ash::features::kDriveFsShowCSEFiles)) {
    return false;
  }
  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  if (!drive_integration_service) {
    return false;
  }
  if (!drive_integration_service->IsMounted()) {
    return false;
  }
  return true;
}

std::unique_ptr<storage::CopyOrMoveHookDelegate>
CopyOrMoveIOTaskImpl::GetHookDelegate(size_t idx) {
  // Using CreateRelayCallback to ensure that the callbacks are executed on the
  // current thread.
  auto progress_callback = google_apis::CreateRelayCallback(
      base::BindRepeating(&CopyOrMoveIOTaskImpl::OnCopyOrMoveProgress,
                          weak_ptr_factory_.GetWeakPtr(), idx));
  auto hook = std::make_unique<FileManagerCopyOrMoveHookDelegate>(
      std::move(progress_callback));

  if (ShouldSkipEncryptedFiles()) {
    auto encryptedHook = std::make_unique<CopyOrMoveEncryptedHookDelegate>(
        profile_,
        base::BindRepeating(&CopyOrMoveIOTaskImpl::OnEncryptedFileSkipped,
                            weak_ptr_factory_.GetWeakPtr(), idx));
    auto combinedHook = storage::CopyOrMoveHookDelegateComposite::CreateOrAdd(
        std::move(hook), std::move(encryptedHook));
    return combinedHook;
  }
  return hook;
}

void CopyOrMoveIOTaskImpl::OnCopyOrMoveProgress(
    size_t idx,
    FileManagerCopyOrMoveHookDelegate::ProgressType type,
    const storage::FileSystemURL& source_url,
    const storage::FileSystemURL& destination_url,
    int64_t size) {
  const std::string destination_path = destination_url.path().AsUTF8Unsafe();
  auto& [individual_progress, aggregate_progress] = item_progresses[idx];

  const auto log_progress = [&]() {
    VLOG(1) << type << "\ncopy_move_src" << source_url.path()
            << "\ncopy_move_des " << destination_url.path();
  };

  using ProgressType = FileManagerCopyOrMoveHookDelegate::ProgressType;
  if (type != ProgressType::kProgress) {
    switch (type) {
      case ProgressType::kBegin:
        log_progress();
        individual_progress[destination_path] = 0;
        return;
      case ProgressType::kEndCopy:
        log_progress();
        individual_progress.erase(destination_path);
        return;
      case ProgressType::kEndMove:
        log_progress();
        individual_progress.erase(destination_path);
        return;
      case ProgressType::kEndRemoveSource:
        log_progress();
        return;
      case ProgressType::kError:
        log_progress();
        return;
      default:
        NOTREACHED_IN_MIGRATION() << "Unknown ProgressType: " << int(type);
        return;
    }
  }

  // The |size| is only valid for ProgressType::kProgress.
  DCHECK_EQ(ProgressType::kProgress, type);
  int64_t& last_size = individual_progress.at(destination_path);
  int64_t delta = size - last_size;
  last_size = size;
  aggregate_progress += delta;

  if (speedometer_.Update(progress_->bytes_transferred += delta)) {
    const base::TimeDelta remaining_time = speedometer_.GetRemainingTime();

    // Speedometer can produce infinite result which can't be serialized to JSON
    // when sending the status via private API.
    if (!remaining_time.is_inf()) {
      progress_->remaining_seconds = remaining_time.InSecondsF();
    }
  }

  progress_callback_.Run(*progress_);
}

void CopyOrMoveIOTaskImpl::OnEncryptedFileSkipped(size_t idx,
                                                  storage::FileSystemURL url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  progress_->skipped_encrypted_files.emplace_back(std::move(url));
  progress_->sources[idx].error = base::File::FILE_ERROR_FAILED;
  progress_->outputs[idx].error = base::File::FILE_ERROR_FAILED;
}

void CopyOrMoveIOTaskImpl::OnCopyOrMoveComplete(size_t idx,
                                                base::File::Error error) {
  DCHECK(idx < progress_->sources.size());
  DCHECK(idx < progress_->outputs.size());

  operation_id_.reset();

  if (!progress_->sources[idx].error) {
    progress_->sources[idx].error = error;
  }
  if (!progress_->outputs[idx].error) {
    progress_->outputs[idx].error = error;
  }

  auto& [individual_progress, aggregate_progress] = item_progresses[idx];
  individual_progress.clear();

  // Some copy and move operations (depending on the source and destination
  // filesystems) don't support progress reporting yet, so we rely on setting
  // bytes_transferred only when each item completes. By also deducting
  // `aggregate_progress` from bytes_transferred, we ensure that both operations
  // that report progress and those that don't are supported.
  progress_->bytes_transferred += source_sizes_[idx] - aggregate_progress;

  if (idx < progress_->sources.size() - 1) {
    progress_callback_.Run(*progress_);
    GenerateDestinationURL(idx + 1);
    return;
  }

  // Complete: assume State::kSuccess.
  file_manager::io_task::State complete_state = State::kSuccess;

  // Look for source errors and set the complete state to State::Error if any
  // source errors are found.
  for (const auto& source : progress_->sources) {
    DCHECK(source.error.has_value());
    if (source.error.value() != base::File::FILE_OK) {
      LOG(ERROR) << "Cannot copy or move " << Redact(source.url) << ": "
                 << base::File::ErrorToString(source.error.value());
      complete_state = State::kError;
      break;
    }
  }

  Complete(complete_state);
}

void CopyOrMoveIOTaskImpl::SetCurrentOperationID(
    storage::FileSystemOperationRunner::OperationID id) {
  operation_id_.emplace(id);
}

}  // namespace file_manager::io_task
