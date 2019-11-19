// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_download_client.h"

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager.h"
#include "chrome/browser/chromeos/plugin_vm/plugin_vm_image_manager_factory.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/download_service.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace plugin_vm {

PluginVmImageDownloadClient::PluginVmImageDownloadClient(Profile* profile)
    : profile_(profile) {}
PluginVmImageDownloadClient::~PluginVmImageDownloadClient() = default;

PluginVmImageManager* PluginVmImageDownloadClient::GetManager() {
  return PluginVmImageManagerFactory::GetForProfile(profile_);
}

// TODO(okalitova): Remove logs.

void PluginVmImageDownloadClient::OnServiceInitialized(
    bool state_lost,
    const std::vector<download::DownloadMetaData>& downloads) {
  VLOG(1) << __func__ << " called";
  // TODO(okalitova): Manage downloads after sleep and log out.
  for (const auto& download : downloads) {
    VLOG(1) << "Download tracked by DownloadService: " << download.guid;
    old_downloads_.insert(download.guid);
    DownloadServiceFactory::GetForKey(profile_->GetProfileKey())
        ->CancelDownload(download.guid);
  }
}

void PluginVmImageDownloadClient::OnServiceUnavailable() {
  VLOG(1) << __func__ << " called";
}

void PluginVmImageDownloadClient::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  VLOG(1) << __func__ << " called";
  // We do not want downloads that are tracked by download service from its
  // initialization to proceed.
  if (old_downloads_.find(guid) != old_downloads_.end()) {
    DownloadServiceFactory::GetForKey(profile_->GetProfileKey())
        ->CancelDownload(guid);
    return;
  }

  content_length_ = headers ? headers->GetContentLength() : -1;
  GetManager()->OnDownloadStarted();
}

void PluginVmImageDownloadClient::OnDownloadUpdated(const std::string& guid,
                                                    uint64_t bytes_uploaded,
                                                    uint64_t bytes_downloaded) {
  DCHECK(old_downloads_.find(guid) == old_downloads_.end());
  VLOG(1) << __func__ << " called";
  VLOG(1) << bytes_downloaded << " bytes downloaded";
  GetManager()->OnDownloadProgressUpdated(bytes_downloaded, content_length_);
}

void PluginVmImageDownloadClient::OnDownloadFailed(
    const std::string& guid,
    const download::CompletionInfo& completion_info,
    download::Client::FailureReason clientReason) {
  VLOG(1) << __func__ << " called";
  auto managerReason =
      PluginVmImageManager::FailureReason::DOWNLOAD_FAILED_UNKNOWN;
  switch (clientReason) {
    case download::Client::FailureReason::NETWORK:
      VLOG(1) << "Failure reason: NETWORK";
      managerReason =
          PluginVmImageManager::FailureReason::DOWNLOAD_FAILED_NETWORK;
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
      managerReason =
          PluginVmImageManager::FailureReason::DOWNLOAD_FAILED_ABORTED;
      break;
    case download::Client::FailureReason::CANCELLED:
      VLOG(1) << "Failure reason: CANCELLED";
      break;
  }

  // We do not want to notify PluginVmImageManager about the status of
  // downloads that are tracked by download service from its initialization.
  if (old_downloads_.find(guid) != old_downloads_.end())
    return;

  if (clientReason == download::Client::FailureReason::CANCELLED)
    GetManager()->OnDownloadCancelled();
  else
    GetManager()->OnDownloadFailed(managerReason);
}

void PluginVmImageDownloadClient::OnDownloadSucceeded(
    const std::string& guid,
    const download::CompletionInfo& completion_info) {
  DCHECK(old_downloads_.find(guid) == old_downloads_.end());
  VLOG(1) << __func__ << " called";
  VLOG(1) << "Downloaded file is in " << completion_info.path.value();
  GetManager()->OnDownloadCompleted(completion_info);
}

bool PluginVmImageDownloadClient::CanServiceRemoveDownloadedFile(
    const std::string& guid,
    bool force_delete) {
  VLOG(1) << __func__ << " called";
  return true;
}

void PluginVmImageDownloadClient::GetUploadData(
    const std::string& guid,
    download::GetUploadDataCallback callback) {
  DCHECK(old_downloads_.find(guid) == old_downloads_.end());
  VLOG(1) << __func__ << " called";
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), nullptr));
}

}  // namespace plugin_vm
