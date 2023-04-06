// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/share_info_file_handler.h"

#include <cinttypes>
#include <string>

#include "ash/components/arc/arc_util.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/ash/arc/nearby_share/arc_nearby_share_uma.h"
#include "chrome/browser/ash/file_manager/fileapi_util.h"
#include "chrome/browser/ash/fileapi/external_file_url_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "url/gurl.h"

// Enable VLOG level 1.
#undef ENABLED_VLOG_LEVEL
#define ENABLED_VLOG_LEVEL 1

namespace arc {

namespace {
constexpr int kStreamReaderBufSizeInBytes = 32 * 1024;

// Since most shareable Android app content lives in a virtual file system,
// it will take some measurable amount of time to stream images / videos
// from ContentProviders in ARC to local files in the Chrome OS filesystem
// before Nearby Share can consume it.  Since we don't have limit on number
// of files or size of files users can share via Nearby Share, set to some
// reasonable number of minutes per GB of transfer.
constexpr base::TimeDelta kFileStreamingTimeoutPerGB = base::Minutes(2);

int64_t GetTimeoutInSecondsFromBytes(uint64_t transfer_bytes) {
  constexpr double kGBInBytes = 1 * 1024 * 1024 * 1024;
  const int64_t transfer_bytes_in_gb = base::checked_cast<int64_t>(
      std::ceil(base::checked_cast<double>(transfer_bytes) / kGBInBytes));

  // Always set timeout to at least |kFileStreamingTimeoutPerGB|.
  return (transfer_bytes_in_gb > 0)
             ? transfer_bytes_in_gb * kFileStreamingTimeoutPerGB.InSeconds()
             : kFileStreamingTimeoutPerGB.InSeconds();
}

// Returns scoped_refptr to FileSystemContext for an url.
scoped_refptr<storage::FileSystemContext> GetScopedFileSystemContext(
    Profile* const profile,
    const GURL& url) {
  content::StoragePartition* const storage =
      profile->content::BrowserContext::GetStoragePartitionForUrl(url);
  DCHECK(storage);
  return storage->GetFileSystemContext();
}

// Converts the given url to a FileSystemURL.
file_manager::util::FileSystemURLAndHandle GetFileSystemURLAndHandle(
    const storage::FileSystemContext& context,
    const GURL& url) {
  // Obtain the absolute path in the file system.
  const base::FilePath virtual_path = ash::ExternalFileURLToVirtualPath(url);
  DCHECK(!virtual_path.empty());
  // Obtain the file system URL.
  return file_manager::util::CreateIsolatedURLFromVirtualPath(
      context, url::Origin(), virtual_path);
}

std::string StripPathComponents(const std::string& file_name) {
  return base::FilePath(file_name).BaseName().AsUTF8Unsafe();
}
}  // namespace

ShareInfoFileHandler::FileShareConfig::FileShareConfig() = default;
ShareInfoFileHandler::FileShareConfig::~FileShareConfig() = default;

ShareInfoFileHandler::ShareInfoFileHandler(
    Profile* profile,
    mojom::ShareIntentInfo* share_info,
    base::FilePath directory,
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : profile_(profile), task_runner_(task_runner) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(task_runner_);
  DCHECK(share_info);

  file_config_.directory = directory;
  if (share_info->files.has_value()) {
    for (const auto& file_info : share_info->files.value()) {
      GURL externalFileUrl = ArcUrlToExternalFileUrl(file_info->content_uri);
      file_config_.external_urls.emplace_back(externalFileUrl);
      file_config_.mime_types.emplace_back(file_info->mime_type);
      file_config_.names.emplace_back(file_info->name);
      file_config_.sizes.emplace_back(file_info->size);
      if (file_info->size > 0) {
        file_config_.total_size +=
            base::checked_cast<uint64_t>(file_info->size);
      }
    }
    file_config_.num_files = file_config_.external_urls.size();
  }
}

ShareInfoFileHandler::~ShareInfoFileHandler() = default;

// static
file_manager::util::FileSystemURLAndHandle
ShareInfoFileHandler::GetFileSystemURL(content::BrowserContext* context,
                                       const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(context);

  Profile* const profile = Profile::FromBrowserContext(context);
  DCHECK(profile);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      GetScopedFileSystemContext(profile, url);
  DCHECK(file_system_context.get());

  return GetFileSystemURLAndHandle(*file_system_context, url);
}

const std::vector<base::FilePath>& ShareInfoFileHandler::GetFilePaths() const {
  return file_config_.paths;
}

const std::vector<std::string>& ShareInfoFileHandler::GetMimeTypes() const {
  return file_config_.mime_types;
}

void ShareInfoFileHandler::StartPreparingFiles(
    StartedCallback started_callback,
    CompletedCallback completed_callback,
    ProgressBarUpdateCallback update_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(started_callback);
  DCHECK(completed_callback);
  DCHECK(update_callback);
  DCHECK(task_runner_);

  started_callback_ = std::move(started_callback);
  completed_callback_ = std::move(completed_callback);
  update_callback_ = std::move(update_callback);
  file_sharing_started_ = true;

  if (!g_browser_process) {
    LOG(ERROR) << "Unexpected null g_browser_process.";
    UpdateNearbyShareDataHandlingFail(DataHandlingResult::kNullGBrowserProcess);
    NotifyFileSharingCompleted(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  // |profile_| needs to be checked with ProfileManager::IsValidProfile
  // before using it.  Abort if profile is not created.
  if (g_browser_process->profile_manager() &&
      !g_browser_process->profile_manager()->IsValidProfile(profile_)) {
    LOG(ERROR) << "Invalid profile: " << profile_->GetProfileUserName();
    UpdateNearbyShareDataHandlingFail(DataHandlingResult::kInvalidProfile);
    NotifyFileSharingCompleted(base::File::FILE_ERROR_INVALID_OPERATION);
    return;
  }

  if (file_config_.directory.empty()) {
    LOG(ERROR) << "Base directory is empty.";
    UpdateNearbyShareDataHandlingFail(DataHandlingResult::kEmptyDirectory);
    NotifyFileSharingCompleted(base::File::FILE_ERROR_NOT_A_DIRECTORY);
    return;
  }

  VLOG(1)
      << "Creating unique directory for share and converting URLs to files.";
  task_runner_->PostTaskAndReplyWithResult(
      FROM_HERE,
      base::BindOnce(&ShareInfoFileHandler::CreateShareDirectory, this),
      base::BindOnce(&ShareInfoFileHandler::OnShareDirectoryPathCreated,
                     weak_ptr_factory_.GetWeakPtr()));
}

base::FilePath ShareInfoFileHandler::CreateShareDirectory() {
  if (!base::PathExists(file_config_.directory)) {
    LOG(ERROR) << "Base directory does not exist: " << file_config_.directory;
    UpdateNearbyShareDataHandlingFail(
        DataHandlingResult::kDirectoryDoesNotExist);
    return base::FilePath();
  }

  // Prepare a temporary share directory to store cached share files.
  base::FilePath temp_dir;
  constexpr char kShareDirPrefix[] = "share-";
  if (!base::CreateTemporaryDirInDir(file_config_.directory, kShareDirPrefix,
                                     &temp_dir) ||
      !base::PathExists(temp_dir)) {
    LOG(ERROR) << "Failed to create unique temp share directory under: "
               << file_config_.directory;
    UpdateNearbyShareDataHandlingFail(
        DataHandlingResult::kFailedToCreateDirectory);
    return base::FilePath();
  }
  return temp_dir;
}

void ShareInfoFileHandler::OnShareDirectoryPathCreated(
    base::FilePath share_dir) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(started_callback_);
  DCHECK(task_runner_);

  if (share_dir.empty()) {
    LOG(ERROR) << "Failed to prepare temp share directory.";
    UpdateNearbyShareDataHandlingFail(
        DataHandlingResult::kFailedPrepTempDirectory);
    NotifyFileSharingCompleted(base::File::FILE_ERROR_FAILED);
    return;
  }

  auto urls_size = file_config_.external_urls.size();
  if (!urls_size) {
    LOG(ERROR) << "External urls are empty.";
    UpdateNearbyShareDataHandlingFail(DataHandlingResult::kEmptyExternalURL);
    NotifyFileSharingCompleted(base::File::FILE_ERROR_INVALID_URL);
    return;
  }

  for (size_t i = 0; i < urls_size; i++) {
    const GURL& url = file_config_.external_urls[i];
    const std::string file_name = file_config_.names[i];
    int64_t file_size = file_config_.sizes[i];

    if (file_size < 0) {
      LOG(ERROR) << "Invalid size provided for file name: " << file_name;
      UpdateNearbyShareDataHandlingFail(
          DataHandlingResult::kInvalidFileNameSize);
      NotifyFileSharingCompleted(base::File::FILE_ERROR_NOT_A_FILE);
      return;
    }

    const base::FilePath dest_file_path =
        share_dir.AppendASCII(StripPathComponents(file_name));

    task_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&ShareInfoFileHandler::CreateFileForWrite, this,
                       dest_file_path),
        base::BindOnce(&ShareInfoFileHandler::OnFileDescriptorCreated,
                       weak_ptr_factory_.GetWeakPtr(), url, dest_file_path,
                       file_size));
  }
  std::move(started_callback_).Run();
}

base::ScopedFD ShareInfoFileHandler::CreateFileForWrite(
    const base::FilePath& file_path) {
  DCHECK(!file_path.empty());

  base::File dest_file(file_path,
                       base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!dest_file.IsValid() || !base::PathExists(file_path)) {
    LOG(ERROR) << "Invalid destination file at path: " << file_path;
    UpdateNearbyShareDataHandlingFail(
        DataHandlingResult::kInvalidDestinationFilePath);
    return base::ScopedFD();
  }

  return base::ScopedFD(dest_file.TakePlatformFile());
}

void ShareInfoFileHandler::OnFileDescriptorCreated(
    const GURL& url,
    const base::FilePath& dest_file_path,
    const int64_t file_size,
    base::ScopedFD dest_fd) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(url.is_valid());
  DCHECK(!dest_file_path.empty());
  DCHECK_GE(file_size, 0);

  if (!dest_fd.is_valid()) {
    LOG(ERROR) << "Invalid destination file descriptor.";
    UpdateNearbyShareDataHandlingFail(
        DataHandlingResult::kInvalidDestinationFileDescriptor);
    NotifyFileSharingCompleted(base::File::FILE_ERROR_FAILED);
    return;
  }

  contexts_.emplace_front();
  auto it_context = contexts_.begin();
  *it_context = GetScopedFileSystemContext(profile_, url);
  DCHECK(it_context->get());

  const file_manager::util::FileSystemURLAndHandle isolated_file_system =
      GetFileSystemURLAndHandle(**it_context, url);

  if (!isolated_file_system.url.is_valid()) {
    LOG(ERROR) << "Invalid FileSystemURL from handle.";
    UpdateNearbyShareDataHandlingFail(
        DataHandlingResult::kInvalidFileSystemURL);
    NotifyFileSharingCompleted(base::File::FILE_ERROR_INVALID_URL);
    return;
  }

  // Check if the obtained path providing external file URL or not.
  if (!ash::IsExternalFileURLType(isolated_file_system.url.type())) {
    LOG(ERROR) << "FileSystemURL is not of external file type.";
    UpdateNearbyShareDataHandlingFail(DataHandlingResult::kNotExternalFileType);
    NotifyFileSharingCompleted(base::File::FILE_ERROR_INVALID_URL);
    return;
  }

  file_stream_adapters_.emplace_front();
  auto it_stream_adapter = file_stream_adapters_.begin();
  *it_stream_adapter = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
      *it_context, isolated_file_system.url, /*offset=*/0, file_size,
      kStreamReaderBufSizeInBytes, std::move(dest_fd),
      base::BindOnce(&ShareInfoFileHandler::OnFileStreamReadCompleted,
                     weak_ptr_factory_.GetWeakPtr(),
                     isolated_file_system.url.DebugString(), it_stream_adapter,
                     isolated_file_system.handle.id(), file_size));
  storage::IsolatedContext::GetInstance()->AddReference(
      isolated_file_system.handle.id());

  (*it_stream_adapter)->StartRunner();
  file_config_.paths.push_back(dest_file_path);

  // Used to measure time duration of file stream transfers. For reference,
  // local testing on caroline for 1.2GB takes around 1 minute.
  file_streaming_started_ = base::TimeTicks::Now();

  const int64_t timeout_seconds =
      GetTimeoutInSecondsFromBytes(GetTotalSizeOfFiles());
  const std::string timeout_message = base::StringPrintf(
      "File streaming did not complete within %" PRId64 " second(s).",
      timeout_seconds);
  if (!file_streaming_timer_.IsRunning()) {
    file_streaming_timer_.Start(
        FROM_HERE, base::Seconds(timeout_seconds),
        base::BindOnce(&ShareInfoFileHandler::OnFileStreamingTimeout,
                       weak_ptr_factory_.GetWeakPtr(), timeout_message));
  }
}

void ShareInfoFileHandler::OnFileStreamReadCompleted(
    const std::string& url_str,
    std::list<scoped_refptr<ShareInfoFileStreamAdapter>>::iterator it_adapter,
    const std::string& file_system_id,
    const int64_t bytes_read,
    bool result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!url_str.empty());
  DCHECK(!file_system_id.empty());
  DCHECK_GT(bytes_read, 0);

  storage::IsolatedContext::GetInstance()->RemoveReference(file_system_id);
  file_stream_adapters_.erase(it_adapter);

  if (!result) {
    LOG(ERROR) << "Failed to stream file IO data using url: " << url_str;
    UpdateNearbyShareDataHandlingFail(
        DataHandlingResult::kFailedStreamFileIOData);
    NotifyFileSharingCompleted(base::File::FILE_ERROR_IO);
    return;
  }

  num_bytes_read_ += base::checked_cast<uint64_t>(bytes_read);
  num_files_streamed_++;

  const uint64_t expected_total_bytes = GetTotalSizeOfFiles();
  const size_t expected_total_files = GetNumberOfFiles();
  DVLOG(1) << "Streamed " << num_bytes_read_ << " of " << expected_total_bytes
           << " bytes for " << num_files_streamed_ << " of "
           << expected_total_files << " files";
  if (update_callback_) {
    update_callback_.Run(base::checked_cast<double>(num_bytes_read_) /
                         expected_total_bytes);
  }

  if (num_files_streamed_ == expected_total_files &&
      num_bytes_read_ >= expected_total_bytes) {
    if (num_bytes_read_ > expected_total_bytes) {
      LOG(ERROR) << "Invalid number of bytes read: " << num_bytes_read_ << " > "
                 << expected_total_bytes;
      UpdateNearbyShareDataHandlingFail(
          DataHandlingResult::kInvalidNumberBytesRead);
      NotifyFileSharingCompleted(base::File::FILE_ERROR_INVALID_OPERATION);
      return;
    }

    // Update file streaming duration UMA metric if transfer was successful.
    const base::TimeDelta file_streaming_duration =
        base::TimeTicks::Now() - file_streaming_started_;
    UpdateNearbyShareFileStreamCompleteTime(file_streaming_duration);

    DVLOG(1) << "OnFileStreamReadCompleted: Completed streaming all files.";
    NotifyFileSharingCompleted(base::File::FILE_OK);
  }
}

void ShareInfoFileHandler::OnFileStreamingTimeout(
    const std::string& timeout_message) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  LOG(ERROR) << timeout_message;
  UpdateNearbyShareDataHandlingFail(DataHandlingResult::kTimeout);
  NotifyFileSharingCompleted(base::File::FILE_ERROR_ABORT);
}

void ShareInfoFileHandler::NotifyFileSharingCompleted(
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Stop active timer and reset sharing params.
  if (file_streaming_timer_.IsRunning()) {
    file_streaming_timer_.Stop();
  }
  num_bytes_read_ = 0;
  num_files_streamed_ = 0;

  // Only call |completed_callback_| if not null and file sharing is in started
  // state to prevent calling more than once.
  if (file_sharing_started_ && completed_callback_) {
    file_sharing_started_ = false;
    std::move(completed_callback_).Run(result);
  }
}

}  // namespace arc
