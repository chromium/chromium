// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/extract_io_task.h"

#include <grp.h>
#include <utility>

#include "base/check_op.h"
#include "base/files/file_enumerator.h"
#include "base/files/file_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/strings/strcat.h"
#include "base/system/sys_info.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/threading/platform_thread.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/file_manager/filesystem_api_util.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/ash/fileapi/file_system_backend.h"
#include "chrome/browser/platform_util.h"
#include "components/services/unzip/content/unzip_service.h"
#include "components/services/unzip/public/mojom/unzipper.mojom.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/cros_system_api/constants/cryptohome.h"
#include "third_party/zlib/google/redact.h"

namespace file_manager {
namespace io_task {

void RecordUmaExtractStatus(ExtractStatus status) {
  UMA_HISTOGRAM_ENUMERATION(kExtractTaskStatusHistogramName, status);
}

ExtractIOTask::ExtractIOTask(
    std::vector<storage::FileSystemURL> source_urls,
    std::string password,
    storage::FileSystemURL parent_folder,
    Profile* profile,
    scoped_refptr<storage::FileSystemContext> file_system_context,
    bool show_notification)
    : IOTask(show_notification),
      source_urls_(std::move(source_urls)),
      password_(std::move(password)),
      parent_folder_(std::move(parent_folder)),
      profile_(profile),
      file_system_context_(std::move(file_system_context)) {
  progress_.type = OperationType::kExtract;
  progress_.state = State::kQueued;
  progress_.SetDestinationFolder(parent_folder_, profile);
  progress_.bytes_transferred = 0;
  progress_.total_bytes = 0;
  // Store all the ZIP files in the selection so we have
  // a proper count of how many need to be extracted.
  for (const storage::FileSystemURL& source_url : source_urls_) {
    const base::FilePath source_path = source_url.path();
    if (source_path.MatchesExtension(".zip") &&
        ash::FileSystemBackend::CanHandleURL(source_url)) {
      progress_.sources.emplace_back(source_url, absl::nullopt);
    }
  }
  sizingCount_ = extractCount_ = progress_.sources.size();
}

ExtractIOTask::~ExtractIOTask() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void ExtractIOTask::ZipListenerCallback(uint64_t bytes) {
  progress_.bytes_transferred += bytes;
  speedometer_.Update(progress_.bytes_transferred);
  const double remaining_seconds = speedometer_.GetRemainingSeconds();

  // Speedometer can produce infinite result which can't be serialized to JSON
  // when sending the status via private API.
  if (std::isfinite(remaining_seconds)) {
    progress_.remaining_seconds = remaining_seconds;
  }
  progress_callback_.Run(progress_);
}

void ExtractIOTask::FinishedExtraction(base::FilePath directory, bool success) {
  if (success) {
    // Open a new window to show the extracted content.
    platform_util::ShowItemInFolder(profile_, directory);
  }
  // Release the unpacker parameters stored for the extraction.
  auto unpacker = unpackers_[directory];
  if (unpacker) {
    unpacker->CleanUp();
    // Wait for the task runner to clean up the UnpackParams object.
    while (!unpacker->CleanUpDone()) {
      // Yield until the cancellation tasks are done.
      base::PlatformThread::Sleep(base::Microseconds(1));
    }
  }
  DCHECK_GT(extractCount_, 0u);
  if (--extractCount_ == 0) {
    progress_.state = success ? State::kSuccess : State::kError;
    RecordUmaExtractStatus(progress_.state == State::kSuccess
                               ? ExtractStatus::kSuccess
                               : ExtractStatus::kUnknownError);
    Complete();
  }
}

absl::optional<gid_t> GetDirectoriesOwnerGid() {
  struct group grp, *result = nullptr;
  std::vector<char> buffer(16384);
  getgrnam_r("chronos-access", &grp, buffer.data(), buffer.size(), &result);
  if (!result) {
    return absl::nullopt;
  }
  return grp.gr_gid;
}

// Recursively walk directory and set 'u+rwx,g+x,o+x'.
bool SetDirectoryPermissions(base::FilePath directory, bool success) {
  // Always set permissions in case of error mid-extract.
  base::FileEnumerator traversal(directory, true,
                                 base::FileEnumerator::DIRECTORIES);
  const absl::optional<gid_t> owner_gid = GetDirectoriesOwnerGid();
  for (base::FilePath current = traversal.Next(); !current.empty();
       current = traversal.Next()) {
    base::SetPosixFilePermissions(current,
                                  base::FILE_PERMISSION_READ_BY_USER |
                                      base::FILE_PERMISSION_WRITE_BY_USER |
                                      base::FILE_PERMISSION_EXECUTE_BY_USER |
                                      base::FILE_PERMISSION_READ_BY_GROUP |
                                      base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                                      base::FILE_PERMISSION_EXECUTE_BY_OTHERS);
    // Might not exist in tests.
    if (owner_gid.has_value()) {
      HANDLE_EINTR(chown(current.value().c_str(), -1, owner_gid.value()));
    }
  }
  return success;
}

void ExtractIOTask::ZipExtractCallback(base::FilePath destination_directory,
                                       bool success) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&SetDirectoryPermissions, destination_directory, success),
      base::BindOnce(&ExtractIOTask::FinishedExtraction,
                     weak_ptr_factory_.GetWeakPtr(), destination_directory));
}

void ExtractIOTask::ExtractIntoNewDirectory(
    base::FilePath destination_directory,
    base::FilePath source_file,
    bool created_ok) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (created_ok) {
    unzip::mojom::UnzipOptionsPtr options =
        unzip::mojom::UnzipOptions::New("auto", password_);
    scoped_refptr<unzip::ZipFileUnpacker> unpacker =
        base::MakeRefCounted<unzip::ZipFileUnpacker>();
    unpacker->Unpack(
        unzip::LaunchUnzipper(), source_file, destination_directory,
        std::move(options),
        base::BindRepeating(&ExtractIOTask::ZipListenerCallback,
                            weak_ptr_factory_.GetWeakPtr()),
        base::BindOnce(&ExtractIOTask::ZipExtractCallback,
                       weak_ptr_factory_.GetWeakPtr(), destination_directory));
    unpackers_.insert({destination_directory, std::move(unpacker)});
  } else {
    LOG(ERROR) << "Cannot create directory "
               << zip::Redact(destination_directory);
  }
}

bool CreateExtractionDirectory(const base::FilePath& destination_directory) {
  bool created_ok = base::CreateDirectory(destination_directory);
  // Make sure the directory is world readable.
  if (created_ok) {
    created_ok = base::SetPosixFilePermissions(
        destination_directory, base::FILE_PERMISSION_READ_BY_USER |
                                   base::FILE_PERMISSION_WRITE_BY_USER |
                                   base::FILE_PERMISSION_EXECUTE_BY_USER |
                                   base::FILE_PERMISSION_READ_BY_GROUP |
                                   base::FILE_PERMISSION_EXECUTE_BY_GROUP |
                                   base::FILE_PERMISSION_EXECUTE_BY_OTHERS);
    // Might not exist in tests.
    const absl::optional<gid_t> owner_gid = GetDirectoriesOwnerGid();
    if (created_ok && owner_gid.has_value()) {
      created_ok = (HANDLE_EINTR(chown(destination_directory.value().c_str(),
                                       -1, owner_gid.value())) == 0);
    }
  }
  return created_ok;
}

void ExtractIOTask::ExtractArchive(
    size_t index,
    base::FileErrorOr<storage::FileSystemURL> destination_result) {
  DCHECK(index < progress_.sources.size());
  const base::FilePath source_file = progress_.sources[index].url.path();
  if (!destination_result.has_value()) {
    ZipExtractCallback(base::FilePath(), false);
  } else {
    const base::FilePath destination_directory =
        destination_result.value().path();
    base::ThreadPool::PostTaskAndReplyWithResult(
        FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
        base::BindOnce(&CreateExtractionDirectory, destination_directory),
        base::BindOnce(&ExtractIOTask::ExtractIntoNewDirectory,
                       weak_ptr_factory_.GetWeakPtr(), destination_directory,
                       source_file));
  }
}

void ExtractIOTask::ExtractAllSources() {
  for (size_t index = 0; index < progress_.sources.size(); ++index) {
    const EntryStatus& source = progress_.sources[index];
    const base::FilePath source_file = source.url.path().BaseName();
    util::GenerateUnusedFilename(
        parent_folder_, source_file.RemoveExtension(), file_system_context_,
        base::BindOnce(&ExtractIOTask::ExtractArchive,
                       weak_ptr_factory_.GetWeakPtr(), index));
  }
}

void ExtractIOTask::GotFreeDiskSpace(int64_t free_space) {
  auto* drive_integration_service =
      drive::util::GetIntegrationServiceByProfile(profile_);
  if (progress_.GetDestinationFolder().filesystem_id() ==
          util::GetDownloadsMountPointName(profile_) ||
      (drive_integration_service &&
       drive_integration_service->GetMountPointPath().IsParent(
           progress_.GetDestinationFolder().path()))) {
    free_space -= cryptohome::kMinFreeSpaceInBytes;
  }

  if (progress_.total_bytes > free_space) {
    progress_.outputs.emplace_back(progress_.GetDestinationFolder(),
                                   base::File::FILE_ERROR_NO_SPACE);
    progress_.state = State::kError;
    RecordUmaExtractStatus(ExtractStatus::kInsufficientDiskSpace);
    Complete();
    return;
  }
  if (have_encrypted_content_ && password_.empty()) {
    if (uses_aes_encryption_) {
      RecordUmaExtractStatus(ExtractStatus::kAesEncrypted);
    } else {
      RecordUmaExtractStatus(ExtractStatus::kPasswordError);
    }
    progress_.state = State::kNeedPassword;
    Complete();
    return;
  }

  speedometer_.SetTotalBytes(progress_.total_bytes);
  ExtractAllSources();
}

void ExtractIOTask::ZipInfoCallback(unzip::mojom::InfoPtr info) {
  DCHECK_GT(extractCount_, 0u);
  if (info->size_is_valid) {
    progress_.total_bytes += info->size;
  }
  have_encrypted_content_ = have_encrypted_content_ || info->is_encrypted;
  uses_aes_encryption_ = info->uses_aes_encryption;

  if (--sizingCount_ == 0) {
    // After getting the size of all the ZIPs, check if we have
    // enough available disk space, and if so, extract them.
    if (!parent_folder_.TypeImpliesPathIsReal()) {
      // Destination is a virtual filesystem, so skip the size check.
      ExtractAllSources();
    } else {
      base::ThreadPool::PostTaskAndReplyWithResult(
          FROM_HERE, {base::MayBlock()},
          base::BindOnce(&base::SysInfo::AmountOfFreeDiskSpace,
                         parent_folder_.path()),
          base::BindOnce(&ExtractIOTask::GotFreeDiskSpace,
                         weak_ptr_factory_.GetWeakPtr()));
    }
  }
}

void ExtractIOTask::GetExtractedSize(base::FilePath source_file) {
  unzip::GetExtractedInfo(unzip::LaunchUnzipper(), source_file,
                          base::BindOnce(&ExtractIOTask::ZipInfoCallback,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void ExtractIOTask::CheckSizeThenExtract() {
  for (const EntryStatus& source : progress_.sources) {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(&ExtractIOTask::GetExtractedSize,
                       weak_ptr_factory_.GetWeakPtr(), source.url.path()));
  }
}

void ExtractIOTask::Execute(IOTask::ProgressCallback progress_callback,
                            IOTask::CompleteCallback complete_callback) {
  progress_callback_ = std::move(progress_callback);
  complete_callback_ = std::move(complete_callback);

  DVLOG(1) << "Executing EXTRACT_ARCHIVE IO task";
  progress_.state = State::kInProgress;
  progress_callback_.Run(progress_);
  // If the backend can't handle the folder to unpack into or
  // there are no files to extract, finish the operation with an error.
  if (!ash::FileSystemBackend::CanHandleURL(parent_folder_) ||
      sizingCount_ == 0) {
    progress_.state = State::kError;
    RecordUmaExtractStatus(ExtractStatus::kUnknownError);
    Complete();
  } else {
    CheckSizeThenExtract();
  }
}

void ExtractIOTask::Cancel() {
  progress_.state = State::kCancelled;
  RecordUmaExtractStatus(ExtractStatus::kCancelled);
  // Run through all existing extraction instances and cancel them all.
  for (auto unpacker : unpackers_) {
    if (unpacker.second) {
      unpacker.second->Stop();
      while (!unpacker.second->CleanUpDone()) {
        // Yield until the UnpackParams objects have been released.
        base::PlatformThread::Sleep(base::Microseconds(1));
      }
    }
  }
}

// Calls the completion callback for the task. |progress_| should not be
// accessed after calling this.
void ExtractIOTask::Complete() {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(std::move(complete_callback_), std::move(progress_)));
}

}  // namespace io_task
}  // namespace file_manager
