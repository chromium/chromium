// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/bruschetta/bruschetta_download_client.h"

#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "components/download/public/background_service/background_download_service.h"
#include "components/download/public/background_service/download_metadata.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace bruschetta {

BruschettaInstaller* BruschettaDownloadClient::installer_ = nullptr;

BruschettaDownloadClient::BruschettaDownloadClient(Profile* profile)
    : profile_(profile) {}

bool BruschettaDownloadClient::MaybeCancelDownload(const std::string& guid) {
  if (!installer_ ||
      guid != installer_->GetDownloadGuid().AsLowercaseString()) {
    BackgroundDownloadServiceFactory::GetForKey(profile_->GetProfileKey())
        ->CancelDownload(guid);
    return true;
  }
  return false;
}

void BruschettaDownloadClient::OnServiceInitialized(
    bool state_lost,
    const std::vector<download::DownloadMetaData>& downloads) {
  for (const auto& download : downloads) {
    MaybeCancelDownload(download.guid);
  }
}

void BruschettaDownloadClient::OnServiceUnavailable() {}

void BruschettaDownloadClient::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  if (MaybeCancelDownload(guid)) {
    return;
  }
}

void BruschettaDownloadClient::OnDownloadUpdated(const std::string& guid,
                                                 uint64_t bytes_uploaded,
                                                 uint64_t bytes_downloaded) {
  if (MaybeCancelDownload(guid)) {
    return;
  }
}

void BruschettaDownloadClient::OnDownloadFailed(
    const std::string& guid,
    const download::CompletionInfo& info,
    FailureReason reason) {
  if (MaybeCancelDownload(guid)) {
    return;
  }
  LOG(ERROR) << "Failed to download object, error code "
             << static_cast<int>(reason);
  installer_->DownloadFailed();
}

void BruschettaDownloadClient::OnDownloadSucceeded(
    const std::string& guid,
    const download::CompletionInfo& completion_info) {
  if (MaybeCancelDownload(guid)) {
    return;
  }
  installer_->DownloadSucceeded(completion_info);
}

bool BruschettaDownloadClient::CanServiceRemoveDownloadedFile(
    const std::string& guid,
    bool force_delete) {
  return !installer_;
}

void BruschettaDownloadClient::GetUploadData(
    const std::string& guid,
    download::GetUploadDataCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), nullptr));
}

void BruschettaDownloadClient::SetInstallerInstance(
    BruschettaInstaller* instance) {
  DCHECK(!installer_ || !instance);
  installer_ = instance;
}

}  // namespace bruschetta
