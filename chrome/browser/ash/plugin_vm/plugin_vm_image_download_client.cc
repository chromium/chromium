// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/plugin_vm/plugin_vm_image_download_client.h"

#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer.h"
#include "chrome/browser/ash/plugin_vm/plugin_vm_installer_factory.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/download_metadata.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace plugin_vm {

PluginVmImageDownloadClient::PluginVmImageDownloadClient(Profile* profile)
    : profile_(profile) {}
PluginVmImageDownloadClient::~PluginVmImageDownloadClient() = default;

PluginVmInstaller* PluginVmImageDownloadClient::GetInstaller() {
  return PluginVmInstallerFactory::GetForProfile(profile_);
}

bool PluginVmImageDownloadClient::IsCurrentDownload(const std::string& guid) {
  return guid == GetInstaller()->GetCurrentDownloadGuid();
}

void PluginVmImageDownloadClient::OnServiceInitialized(
    bool state_lost,
    const std::vector<download::DownloadMetaData>& downloads) {
  // TODO(timloh): It appears that only completed downloads (aka previous
  // successful installations) surface here, so this logic might not be needed.
  for (const auto& download : downloads) {
    VLOG(1) << "Download tracked by BackgroundDownloadService: "
            << download.guid;
    BackgroundDownloadServiceFactory::GetForKey(profile_->GetProfileKey())
        ->CancelDownload(download.guid);
  }
}

void PluginVmImageDownloadClient::OnServiceUnavailable() {
}

void PluginVmImageDownloadClient::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  // We do not want downloads that are tracked by download service from its
  // initialization to proceed.
  if (!IsCurrentDownload(guid)) {
    BackgroundDownloadServiceFactory::GetForKey(profile_->GetProfileKey())
        ->CancelDownload(guid);
    return;
  }

  content_length_ = headers ? headers->GetContentLength() : -1;
  response_code_ = headers ? headers->response_code() : -1;

  GetInstaller()->OnDownloadStarted();
}

void PluginVmImageDownloadClient::OnDownloadUpdated(const std::string& guid,
                                                    uint64_t bytes_uploaded,
                                                    uint64_t bytes_downloaded) {
  DCHECK(IsCurrentDownload(guid));
  VLOG(1) << bytes_downloaded << " bytes downloaded";
  GetInstaller()->OnDownloadProgressUpdated(bytes_downloaded, content_length_);
}

void PluginVmImageDownloadClient::OnDownloadFailed(
    const std::string& guid,
    const download::CompletionInfo& completion_info,
    download::Client::FailureReason clientReason) {
  auto failureReason =
      PluginVmInstaller::FailureReason::DOWNLOAD_FAILED_UNKNOWN;
  switch (clientReason) {
    case download::Client::FailureReason::NETWORK:
      VLOG(1) << "Failure reason: NETWORK, response_code: " << response_code_;
      if (response_code_ == 401) {
        failureReason = PluginVmInstaller::FailureReason::DOWNLOAD_FAILED_401;
      } else if (response_code_ == 403) {
        failureReason = PluginVmInstaller::FailureReason::DOWNLOAD_FAILED_403;
      } else if (response_code_ == 404) {
        failureReason = PluginVmInstaller::FailureReason::DOWNLOAD_FAILED_404;
      } else {
        failureReason =
            PluginVmInstaller::FailureReason::DOWNLOAD_FAILED_NETWORK;
      }
      break;
    case download::Client::FailureReason::UPLOAD_TIMEDOUT:
      VLOG(1) << "Failure reason: UPLOAD_TIMEDOUT";
      break;
    case download::Client::FailureReason::TIMEDOUT:
      VLOG(1) << "Failure reason: TIMEDOUT";
      break;
    case download::Client::FailureReason::UNKNOWN:
      VLOG(1) << "Failure reason: UNKNOWN";
      break;
    case download::Client::FailureReason::ABORTED:
      VLOG(1) << "Failure reason: ABORTED";
      failureReason = PluginVmInstaller::FailureReason::DOWNLOAD_FAILED_ABORTED;
      break;
    case download::Client::FailureReason::CANCELLED:
      VLOG(1) << "Failure reason: CANCELLED";
      break;
  }

  if (!IsCurrentDownload(guid))
    return;

  GetInstaller()->OnDownloadFailed(failureReason);
}

void PluginVmImageDownloadClient::OnDownloadSucceeded(
    const std::string& guid,
    const download::CompletionInfo& completion_info) {
  DCHECK(IsCurrentDownload(guid));
  VLOG(1) << "Downloaded file is in " << completion_info.path.value();
  GetInstaller()->OnDownloadCompleted(completion_info);
}

bool PluginVmImageDownloadClient::CanServiceRemoveDownloadedFile(
    const std::string& guid,
    bool force_delete) {
  return true;
}

void PluginVmImageDownloadClient::GetUploadData(
    const std::string& guid,
    download::GetUploadDataCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), nullptr));
}

}  // namespace plugin_vm
