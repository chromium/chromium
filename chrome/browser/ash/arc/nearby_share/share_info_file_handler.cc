// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/nearby_share/share_info_file_handler.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/file_util.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/numerics/checked_math.h"
#include "base/strings/stringprintf.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "chrome/browser/ash/arc/fileapi/arc_content_file_system_url_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/file_manager/fileapi_util.h"
#include "chrome/browser/chromeos/fileapi/external_file_url_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/arc/arc_util.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/storage_partition.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/isolated_context.h"
#include "url/gurl.h"

namespace arc {

namespace {
constexpr int kStreamReaderBufSizeInBytes = 32 * 1024;

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
file_manager::util::FileSystemURLAndHandle GetFileSystemURL(
    const storage::FileSystemContext& context,
    const GURL& url) {
  // Obtain the absolute path in the file system.
  const base::FilePath virtual_path =
      chromeos::ExternalFileURLToVirtualPath(url);
  DCHECK(!virtual_path.empty());
  // Obtain the file system URL.
  return file_manager::util::CreateIsolatedURLFromVirtualPath(
      context, /* empty origin */ GURL(), virtual_path);
}

std::string StripPathComponents(const std::string& file_name) {
  return base::FilePath(file_name).BaseName().AsUTF8Unsafe();
}
}  // namespace

ShareInfoFileHandler::FileShareConfig::FileShareConfig() = default;
ShareInfoFileHandler::FileShareConfig::~FileShareConfig() = default;

ShareInfoFileHandler::~ShareInfoFileHandler() = default;

ShareInfoFileHandler::ShareInfoFileHandler(Profile* profile,
                                           mojom::ShareIntentInfo* share_info,
                                           base::FilePath directory)
    : profile_(profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(profile_);
  DCHECK(share_info);

  file_config_.directory = directory;
  if (share_info->files.has_value()) {
    for (const auto& file_info : share_info->files.value()) {
      GURL externalFileUrl = ArcUrlToExternalFileUrl(file_info->content_uri);
      file_config_.external_urls.emplace_back(externalFileUrl);
      file_config_.mime_types.emplace_back(file_info->mime_type);
      file_config_.names.emplace_back(file_info->name);
      file_config_.sizes.emplace_back(file_info->size);
      file_config_.total_size += base::checked_cast<uint64_t>(file_info->size);
    }
  }
}

file_manager::util::FileSystemURLAndHandle GetFileSystemContext(
    content::BrowserContext* context,
    const GURL& url) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(context);

  Profile* const profile = Profile::FromBrowserContext(context);
  DCHECK(profile);

  scoped_refptr<storage::FileSystemContext> file_system_context =
      GetScopedFileSystemContext(profile, url);
  DCHECK(file_system_context.get());

  return GetFileSystemURL(*file_system_context, url);
}

const std::vector<base::FilePath>& ShareInfoFileHandler::GetFilePaths() const {
  return file_config_.paths;
}

const std::vector<std::string>& ShareInfoFileHandler::GetMimeTypes() const {
  return file_config_.mime_types;
}

uint64_t ShareInfoFileHandler::GetTotalSizeOfFiles() const {
  return file_config_.total_size;
}

void ShareInfoFileHandler::StartPreparingFiles(CompletedCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  if (!base::PathExists(file_config_.directory)) {
    LOG(ERROR) << "Share directory does not exist: " << file_config_.directory;
    std::move(callback).Run(false);
  }

  if (!g_browser_process) {
    LOG(ERROR) << "Unexpected null g_browser_process";
    std::move(callback).Run(false);
  }

  // |profile_| needs to be checked with ProfileManager::IsValidProfile
  // before using it.  Abort if profile is not created.
  if (g_browser_process->profile_manager() &&
      !g_browser_process->profile_manager()->IsValidProfile(profile_)) {
    LOG(ERROR) << "Invalid profile: " << profile_->GetProfileUserName();
    std::move(callback).Run(false);
  }

  if (file_config_.directory.empty()) {
    LOG(ERROR) << "Base directory is empty.";
    std::move(callback).Run(false);
  }

  VLOG(1) << "Creating unique directory for share and converting URLs to files";
  base::ThreadPool::PostTaskAndReplyWithResult(
      // USER_VISIBLE because of downloading files requested by the user and
      // will help update UI on progress of transfers.
      FROM_HERE, {base::MayBlock(), base::TaskPriority::USER_VISIBLE},
      base::BindOnce(&ShareInfoFileHandler::CreateDirectoryAndStreamFiles,
                     this),
      base::BindOnce(&ShareInfoFileHandler::OnCreatedDirectoryAndStreamedFiles,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

bool ShareInfoFileHandler::CreateDirectoryAndStreamFiles() {
  // Keep track of all scoped directories created so they can be cleaned up.
  scoped_temp_dirs_.emplace_front();
  auto it_temp_dir = scoped_temp_dirs_.begin();

  // Prepare a temporary directory and the destination file.
  if (!it_temp_dir->CreateUniqueTempDirUnderPath(file_config_.directory)) {
    LOG(ERROR) << "Failed to create unique temp directory for: "
               << file_config_.directory;
    return false;
  }

  auto urls_size = file_config_.external_urls.size();
  if (!urls_size) {
    LOG(ERROR) << "External urls are empty.";
    return false;
  }

  for (auto i = 0; i < urls_size; i++) {
    const GURL& url = file_config_.external_urls[i];
    const std::string file_name = file_config_.names[i];
    int64_t file_size = file_config_.sizes[i];

    if (file_size < 0) {
      LOG(ERROR) << "Invalid size provided for file name: " << file_name;
      return false;
    }

    contexts_.emplace_front();
    auto it_context = contexts_.begin();
    *it_context = GetScopedFileSystemContext(profile_, url);
    DCHECK(it_context->get());

    const file_manager::util::FileSystemURLAndHandle isolated_file_system =
        GetFileSystemURL(**it_context, url);

    if (!isolated_file_system.url.is_valid()) {
      LOG(ERROR) << "Invalid FileSystemURL from handle.";
      return false;
    }

    // Check if the obtained path providing external file URL or not.
    if (!chromeos::IsExternalFileURLType(isolated_file_system.url.type())) {
      LOG(ERROR) << "FileSystemURL is not of external file type.";
      return false;
    }

    const base::FilePath dest_file_path =
        it_temp_dir->GetPath().AppendASCII(StripPathComponents(file_name));

    auto dest_fd = CreateFileForWrite(dest_file_path);
    if (!dest_fd.is_valid()) {
      LOG(ERROR) << "Invalid destination file descriptor.";
      return false;
    }

    file_stream_adapters_.emplace_front();
    auto it_stream_adapter = file_stream_adapters_.begin();
    *it_stream_adapter = base::MakeRefCounted<ShareInfoFileStreamAdapter>(
        *it_context, isolated_file_system.url, 0 /* offset */, file_size,
        kStreamReaderBufSizeInBytes, std::move(dest_fd),
        base::BindOnce(&ShareInfoFileHandler::OnFileStreamReadCompleted,
                       weak_ptr_factory_.GetWeakPtr(),
                       isolated_file_system.url.DebugString(),
                       it_stream_adapter, file_size));
    (*it_stream_adapter)->StartRunner();

    file_config_.paths.push_back(dest_file_path);
  }
  return true;
}

base::ScopedFD ShareInfoFileHandler::CreateFileForWrite(
    const base::FilePath& file_path) {
  base::File dest_file(file_path,
                       base::File::FLAG_CREATE | base::File::FLAG_WRITE);
  if (!dest_file.IsValid() || !base::PathExists(file_path)) {
    LOG(ERROR) << "Invalid destination file at path: " << file_path;
    return base::ScopedFD();
  }

  return base::ScopedFD(dest_file.TakePlatformFile());
}

void ShareInfoFileHandler::OnCreatedDirectoryAndStreamedFiles(
    CompletedCallback callback,
    bool result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  VLOG(1) << "Called OnCreatedDirectoryAndStreamedFiles";
  if (!result) {
    LOG(ERROR) << "Failed to prepare temp directory and stream files.";
    std::move(callback).Run(false);
  }

  std::move(callback).Run(true);
}

void ShareInfoFileHandler::OnFileStreamReadCompleted(
    const std::string& url_str,
    std::list<scoped_refptr<ShareInfoFileStreamAdapter>>::iterator it,
    const int64_t bytes_read,
    bool result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  VLOG(1) << "Called OnFileStreamReadCompleted";
  LOG_IF(ERROR, !result) << "Failed to read from url " << url_str;
  DCHECK_GT(bytes_read, 0);

  file_stream_adapters_.erase(it);

  num_bytes_read_ += bytes_read;
  // TODO(alanding): Update progress bar UI and add UMA metric.
  // UpdateProgressBar(num_bytes_read_/file_config_.total_size);
}

}  // namespace arc
