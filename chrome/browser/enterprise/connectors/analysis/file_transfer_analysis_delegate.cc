// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/file_transfer_analysis_delegate.h"

#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "chrome/browser/ash/file_manager/volume_manager.h"
#include "chrome/browser/enterprise/connectors/analysis/files_request_handler.h"
#include "chrome/browser/enterprise/connectors/connectors_service.h"
#include "chrome/browser/extensions/api/safe_browsing_private/safe_browsing_private_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "storage/browser/file_system/file_system_context.h"
#include "storage/browser/file_system/file_system_operation.h"
#include "storage/browser/file_system/file_system_operation_runner.h"
#include "storage/browser/file_system/file_system_url.h"
#include "storage/browser/file_system/recursive_operation_delegate.h"

using safe_browsing::BinaryUploadService;

namespace {
// GetFileURLsDelegate is used to get the `FileSystemURL`s of all files lying
// within `root`. A vector of these urls is passed to `callback`. If `root` is
// a file, the vector will only contain `root`. If `root` is a directory all
// files lying in that directory or any descended subdirectory are passed to
// `callback`.
class GetFileURLsDelegate : public storage::RecursiveOperationDelegate {
 public:
  using FileURLsCallback =
      base::OnceCallback<void(std::vector<storage::FileSystemURL>)>;

  GetFileURLsDelegate(storage::FileSystemContext* file_system_context,
                      const storage::FileSystemURL& root,
                      FileURLsCallback callback)
      : RecursiveOperationDelegate(file_system_context),
        root_(root),
        callback_(std::move(callback)) {}

  GetFileURLsDelegate(const GetFileURLsDelegate&) = delete;
  GetFileURLsDelegate& operator=(const GetFileURLsDelegate&) = delete;

  ~GetFileURLsDelegate() override = default;

  // RecursiveOperationDelegate:
  void Run() override { NOTREACHED(); }
  void RunRecursively() override {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
    StartRecursiveOperation(root_,
                            storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
                            base::BindOnce(&GetFileURLsDelegate::Completed,
                                           weak_ptr_factory_.GetWeakPtr()));
  }
  void ProcessFile(const storage::FileSystemURL& url,
                   StatusCallback callback) override {
    if (error_url_.is_valid() && error_url_ == url) {
      std::move(callback).Run(base::File::FILE_ERROR_FAILED);
      return;
    }

    file_system_context()->operation_runner()->GetMetadata(
        url, storage::FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY,
        base::BindOnce(&GetFileURLsDelegate::OnGetMetadata,
                       weak_ptr_factory_.GetWeakPtr(), url,
                       std::move(callback)));
  }
  void ProcessDirectory(const storage::FileSystemURL& url,
                        StatusCallback callback) override {
    std::move(callback).Run(base::File::FILE_OK);
  }
  void PostProcessDirectory(const storage::FileSystemURL& url,
                            StatusCallback callback) override {
    std::move(callback).Run(base::File::FILE_OK);
  }

 private:
  void OnGetMetadata(const storage::FileSystemURL& url,
                     StatusCallback callback,
                     base::File::Error result,
                     const base::File::Info& file_info) {
    if (result != base::File::FILE_OK) {
      std::move(callback).Run(result);
      return;
    }
    if (file_info.is_directory) {
      std::move(callback).Run(base::File::FILE_ERROR_NOT_A_FILE);
      return;
    }

    file_urls_.push_back(url);
    std::move(callback).Run(base::File::FILE_OK);
  }

  void Completed(base::File::Error result) {
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE, base::BindOnce(std::move(callback_), std::move(file_urls_)));
  }

  storage::FileSystemURL root_;
  FileURLsCallback callback_;
  storage::FileSystemURL error_url_;
  std::vector<storage::FileSystemURL> file_urls_;

  base::WeakPtrFactory<GetFileURLsDelegate> weak_ptr_factory_{this};
};

bool IsInSameFileSystem(Profile* profile,
                        storage::FileSystemURL source_url,
                        storage::FileSystemURL destination_url) {
  // Cheap check: source file system url.
  if (!source_url.IsInSameFileSystem(destination_url))
    return false;

  // For some URLs FileSystemURL's IsInSameFileSystem function returns false
  // positives. Which `volume_manager` is able to properly determine.
  file_manager::VolumeManager* const volume_manager =
      file_manager::VolumeManager::Get(profile);
  base::WeakPtr<file_manager::Volume> source_volume =
      volume_manager->FindVolumeFromPath(source_url.path());
  base::WeakPtr<file_manager::Volume> destination_volume =
      volume_manager->FindVolumeFromPath(destination_url.path());

  if (!source_volume || !destination_volume) {
    // The source or destination volume don't exist, so we trust the
    // FileSystemURL response, i.e., they lie in the same file system.
    return true;
  }
  // If both volumes exist, we check whether their ID is the same.
  return source_volume->volume_id() == destination_volume->volume_id();
}

}  // namespace

namespace enterprise_connectors {

absl::optional<AnalysisSettings> FileTransferAnalysisDelegate::IsEnabled(
    Profile* profile,
    storage::FileSystemURL source_url,
    storage::FileSystemURL destination_url) {
  DCHECK(profile);
  auto* service =
      enterprise_connectors::ConnectorsServiceFactory::GetForBrowserContext(
          profile);
  // If the corresponding Connector policy isn't set, don't perform scans.
  if (!service ||
      !service->IsConnectorEnabled(enterprise_connectors::FILE_TRANSFER)) {
    return absl::nullopt;
  }

  if (IsInSameFileSystem(profile, source_url, destination_url)) {
    // Scanning is disabled for transfers on the same file system.
    return absl::nullopt;
  }

  return service->GetAnalysisSettings(source_url, destination_url,
                                      enterprise_connectors::FILE_TRANSFER);
}

FileTransferAnalysisDelegate::FileTransferAnalysisResult
FileTransferAnalysisDelegate::GetAnalysisResultAfterScan(
    storage::FileSystemURL url) {
  for (size_t i = 0; i < source_urls_.size(); ++i) {
    if (source_urls_[i] == url) {
      // TODO(crbug.com/1340312): Support warning mode.
      return results_[i].complies ? FileTransferAnalysisResult::RESULT_ALLOWED
                                  : FileTransferAnalysisResult::RESULT_BLOCKED;
    }
  }
  return FileTransferAnalysisResult::RESULT_UNKNOWN;
}

void FileTransferAnalysisDelegate::UploadData() {
  // This will start aggregating the needed file urls and pass them to
  // OnGotFileSourceURLs.
  // The usage of the WeakPtr is only safe if `get_file_urls_delegate_` is
  // deleted on the IOThread.
  content::GetIOThreadTaskRunner({})->PostTask(
      FROM_HERE,
      base::BindOnce(&storage::RecursiveOperationDelegate::RunRecursively,
                     get_file_urls_delegate_->AsWeakPtr()));
}

FileTransferAnalysisDelegate::FileTransferAnalysisDelegate(
    safe_browsing::DeepScanAccessPoint access_point,
    storage::FileSystemURL source_url,
    storage::FileSystemURL destination_url,
    Profile* profile,
    storage::FileSystemContext* file_system_context,
    AnalysisSettings settings,
    base::OnceClosure callback)
    : settings_{std::move(settings)},
      profile_{profile},
      access_point_{access_point},
      destination_url_{std::move(destination_url)},
      callback_{std::move(callback)} {
  DCHECK(profile);
  DCHECK(!callback_.is_null());

  get_file_urls_delegate_ = std::make_unique<GetFileURLsDelegate>(
      file_system_context, source_url,
      base::BindOnce(&FileTransferAnalysisDelegate::OnGotFileSourceURLs,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FileTransferAnalysisDelegate::BypassWarnings(
    absl::optional<std::u16string> user_justification) {
  // TODO(crbug.com/1340312)
}
void FileTransferAnalysisDelegate::Cancel(bool warning) {
  // TODO(crbug.com/1340313)
}
absl::optional<std::u16string> FileTransferAnalysisDelegate::GetCustomMessage()
    const {
  // TODO(crbug.com/1340312)
  return absl::nullopt;
}
absl::optional<GURL> FileTransferAnalysisDelegate::GetCustomLearnMoreUrl()
    const {
  // TODO(crbug.com/1340312)
  return absl::nullopt;
}
bool FileTransferAnalysisDelegate::BypassRequiresJustification() const {
  // TODO(crbug.com/1340312)
  return false;
}
std::u16string FileTransferAnalysisDelegate::GetBypassJustificationLabel()
    const {
  // TODO(crbug.com/1340312)
  return u"";
}
absl::optional<std::u16string>
FileTransferAnalysisDelegate::OverrideCancelButtonText() const {
  // TODO(crbug.com/1340313)
  return absl::nullopt;
}

FilesRequestHandler*
FileTransferAnalysisDelegate::GetFilesRequestHandlerForTesting() {
  return request_handler_.get();
}

void FileTransferAnalysisDelegate::OnGotFileSourceURLs(
    std::vector<storage::FileSystemURL> source_urls) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  source_urls_ = std::move(source_urls);
  if (source_urls_.empty()) {
    ContentAnalysisCompleted(std::vector<RequestHandlerResult>());
    return;
  }

  std::vector<base::FilePath> paths;
  for (const storage::FileSystemURL& url : source_urls_) {
    paths.push_back(url.path());
  }

  request_handler_ = FilesRequestHandler::Create(
      safe_browsing::BinaryUploadService::GetForProfile(profile_, settings_),
      profile_, settings_, GURL{}, access_point_, std::move(paths),
      base::BindOnce(&FileTransferAnalysisDelegate::ContentAnalysisCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
  request_handler_->UploadData();
}

FileTransferAnalysisDelegate::~FileTransferAnalysisDelegate() {
  if (get_file_urls_delegate_) {
    // To ensure that there are no race conditions, we post the deletion of
    // `get_file_urls_delegate_` to the IO thread.
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](std::unique_ptr<storage::RecursiveOperationDelegate> delegate) {
              // Do nothing.
              // At the end of this task `get_file_urls_delegate_`
              // will be deleted.
            },
            std::move(get_file_urls_delegate_)));
  }
}

void FileTransferAnalysisDelegate::ContentAnalysisCompleted(
    std::vector<RequestHandlerResult> results) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  results_ = std::move(results);
  DCHECK(!callback_.is_null());
  std::move(callback_).Run();
}

}  // namespace enterprise_connectors
