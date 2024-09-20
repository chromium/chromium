// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_drive_image_download_service.h"

#include <stdint.h>

#include <memory>
#include <string>

#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "base/task/thread_pool.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/drive/service/drive_api_service.h"
#include "components/drive/service/drive_service_interface.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/storage_partition.h"
#include "crypto/secure_hash.h"
#include "google_apis/common/api_error_codes.h"
#include "google_apis/drive/drive_common_callbacks.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "url/gurl.h"

namespace plugin_vm {

namespace {

void CreateTemporaryDriveDownloadFile(const base::FilePath& drive_directory,
                                      base::FilePath* file_path) {
  if (!base::DeletePathRecursively(drive_directory)) {
    LOG(ERROR) << "PluginVM Drive download folder failed to be removed";
  }

  const bool has_created_temporary_dir =
      base::CreateDirectoryAndGetError(drive_directory, nullptr);

  if (has_created_temporary_dir)
    base::CreateTemporaryFileInDir(drive_directory, file_path);
}

// 2xx and 3xx error codes indicate success, the rest indicate failure.
bool ErrorCodeIndicatesFailure(google_apis::ApiErrorCode error_code) {
  switch (error_code) {
    case google_apis::HTTP_SUCCESS:
    case google_apis::HTTP_CREATED:
    case google_apis::HTTP_NO_CONTENT:
    case google_apis::HTTP_FOUND:
    case google_apis::HTTP_NOT_MODIFIED:
    case google_apis::HTTP_RESUME_INCOMPLETE:
      return false;
    default:
      return true;
  }
}

// Converts a ApiErrorCode to the closest equivalent FailureReason.
// Do not call with 2xx and 3xx error codes.
plugin_vm::PluginVmInstaller::FailureReason ConvertToFailureReason(
    google_apis::ApiErrorCode error_code) {
  using FailureReason = plugin_vm::PluginVmInstaller::FailureReason;

  switch (error_code) {
    case google_apis::HTTP_BAD_REQUEST:
    case google_apis::HTTP_NOT_FOUND:
    case google_apis::HTTP_CONFLICT:
    case google_apis::HTTP_GONE:
    case google_apis::PARSE_ERROR:
    case google_apis::DRIVE_FILE_ERROR:
      return FailureReason::INVALID_IMAGE_URL;
    case google_apis::HTTP_UNAUTHORIZED:
    case google_apis::HTTP_FORBIDDEN:
    case google_apis::HTTP_LENGTH_REQUIRED:
    case google_apis::HTTP_PRECONDITION:
    case google_apis::HTTP_INTERNAL_SERVER_ERROR:
    case google_apis::HTTP_NOT_IMPLEMENTED:
    case google_apis::HTTP_BAD_GATEWAY:
    case google_apis::HTTP_SERVICE_UNAVAILABLE:
    case google_apis::OTHER_ERROR:
    case google_apis::NOT_READY:
    case google_apis::DRIVE_NO_SPACE:
    case google_apis::DRIVE_RESPONSE_TOO_LARGE:
      return FailureReason::DOWNLOAD_FAILED_UNKNOWN;
    case google_apis::CANCELLED:
      return FailureReason::DOWNLOAD_FAILED_ABORTED;
    case google_apis::NO_CONNECTION:
      return FailureReason::DOWNLOAD_FAILED_NETWORK;
    default:
      NOTREACHED_IN_MIGRATION();
      // This is only used to avoid compiler warnings, it is
      // not actually reachable.
      return FailureReason::DOWNLOAD_FAILED_UNKNOWN;
  }
}
}  // namespace

const char kPluginVmDriveDownloadDirectory[] =
    "/home/chronos/user/PluginVM Drive Download";

PluginVmDriveImageDownloadService::~PluginVmDriveImageDownloadService() =
    default;

PluginVmDriveImageDownloadService::PluginVmDriveImageDownloadService(
    PluginVmInstaller* plugin_vm_installer,
    Profile* profile)
    : plugin_vm_installer_(plugin_vm_installer),
      hasher_(crypto::SecureHash::Create(crypto::SecureHash::SHA256)) {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile);

  scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory =
      profile->GetDefaultStoragePartition()
          ->GetURLLoaderFactoryForBrowserProcess();

  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner =
      base::ThreadPool::CreateSequencedTaskRunner(
          {base::MayBlock(), base::TaskPriority::USER_VISIBLE,
           base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN});

  GURL base_url(GaiaUrls::GetInstance()->google_apis_origin_url());
  GURL base_thumbnail_url(
      google_apis::DriveApiUrlGenerator::kBaseThumbnailUrlForProduction);
  drive_service_ = std::make_unique<drive::DriveAPIService>(
      identity_manager, url_loader_factory, blocking_task_runner.get(),
      base_url, base_thumbnail_url, std::string{},
      kPluginVmNetworkTrafficAnnotation);
  drive_service_->Initialize(
      identity_manager->GetPrimaryAccountId(signin::ConsentLevel::kSignin));
}

void PluginVmDriveImageDownloadService::StartDownload(
    const std::string& file_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  file_id_ = file_id;
  download_file_path_.clear();

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE, {base::TaskPriority::USER_VISIBLE, base::MayBlock()},
      base::BindOnce(&CreateTemporaryDriveDownloadFile, download_directory_,
                     &download_file_path_),
      base::BindOnce(&PluginVmDriveImageDownloadService::DispatchDownloadFile,
                     weak_ptr_factory_.GetWeakPtr()));
}

void PluginVmDriveImageDownloadService::DispatchDownloadFile() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (base::PathExists(download_file_path_)) {
    cancel_callback_ = drive_service_->DownloadFile(
        download_file_path_, file_id_,
        base::BindRepeating(
            &PluginVmDriveImageDownloadService::DownloadActionCallback,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(
            &PluginVmDriveImageDownloadService::GetContentCallback,
            weak_ptr_factory_.GetWeakPtr()),
        base::BindRepeating(
            &PluginVmDriveImageDownloadService::ProgressCallback,
            weak_ptr_factory_.GetWeakPtr()));

    plugin_vm_installer_->OnDownloadStarted();
  } else {
    plugin_vm_installer_->OnDownloadFailed(
        PluginVmInstaller::FailureReason::DOWNLOAD_FAILED_UNKNOWN);
  }
}

void PluginVmDriveImageDownloadService::CancelDownload() {
  DCHECK(cancel_callback_);
  std::move(cancel_callback_).Run();
}

void PluginVmDriveImageDownloadService::ResetState() {
  hasher_ = crypto::SecureHash::Create(crypto::SecureHash::SHA256);
  total_bytes_downloaded_ = 0;
}

void PluginVmDriveImageDownloadService::RemoveTemporaryArchive(
    OnFileDeletedCallback on_file_deleted_callback) {
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, {base::TaskPriority::BEST_EFFORT, base::MayBlock()},
      base::BindOnce(&base::DeletePathRecursively, download_directory_),
      std::move(on_file_deleted_callback));
}

void PluginVmDriveImageDownloadService::SetDriveServiceForTesting(
    std::unique_ptr<drive::DriveServiceInterface> drive_service) {
  drive_service_ = std::move(drive_service);
}

void PluginVmDriveImageDownloadService::SetDownloadDirectoryForTesting(
    const base::FilePath& download_directory) {
  download_directory_ = download_directory;
}

void PluginVmDriveImageDownloadService::DownloadActionCallback(
    google_apis::ApiErrorCode error_code,
    const base::FilePath& file_path) {
  if (ErrorCodeIndicatesFailure(error_code)) {
    LOG(ERROR) << "PluginVM image download from Drive failed with error code: "
               << (int)error_code;
    plugin_vm_installer_->OnDownloadFailed(ConvertToFailureReason(error_code));
    return;
  }

  // We only need .path, .hash256, and .bytes_downloaded as the other fields are
  // not used by PluginVmInstaller::OnDownloadCompleted.
  download::CompletionInfo completion_info;
  completion_info.path = download_file_path_;
  completion_info.bytes_downloaded = total_bytes_downloaded_;
  std::array<uint8_t, 32> sha256_hash;
  hasher_->Finish(sha256_hash);
  completion_info.hash256 = base::HexEncode(sha256_hash);
  plugin_vm_installer_->OnDownloadCompleted(completion_info);
}

void PluginVmDriveImageDownloadService::GetContentCallback(
    google_apis::ApiErrorCode error_code,
    std::unique_ptr<std::string> content,
    bool first_chunk) {
  if (ErrorCodeIndicatesFailure(error_code)) {
    LOG(ERROR) << "Download failed with error code: " << (int)error_code;
    plugin_vm_installer_->OnDownloadFailed(ConvertToFailureReason(error_code));
    return;
  }

  if (first_chunk)
    ResetState();

  hasher_->Update(base::as_byte_span(*content));
  total_bytes_downloaded_ += content->length();
}

void PluginVmDriveImageDownloadService::ProgressCallback(int64_t progress,
                                                         int64_t total) {
  plugin_vm_installer_->OnDownloadProgressUpdated(progress, total);
}

}  // namespace plugin_vm
