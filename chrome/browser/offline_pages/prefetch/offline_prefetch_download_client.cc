// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/offline_pages/prefetch/offline_prefetch_download_client.h"

#include <limits>
#include <map>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/download/download_service_factory.h"
#include "chrome/browser/offline_pages/prefetch/prefetch_service_factory.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/offline_pages/core/prefetch/prefetch_downloader.h"
#include "components/offline_pages/core/prefetch/prefetch_service.h"
#include "services/network/public/cpp/resource_request_body.h"

namespace offline_pages {

OfflinePrefetchDownloadClient::OfflinePrefetchDownloadClient(
    SimpleFactoryKey* simple_factory_key)
    : simple_factory_key_(simple_factory_key) {}

OfflinePrefetchDownloadClient::~OfflinePrefetchDownloadClient() = default;

void OfflinePrefetchDownloadClient::OnServiceInitialized(
    bool state_lost,
    const std::vector<download::DownloadMetaData>& downloads) {
  std::set<std::string> outstanding_download_guids;
  std::map<std::string, std::pair<base::FilePath, int64_t>> success_downloads;
  for (const auto& download : downloads) {
    if (!download.completion_info.has_value()) {
      outstanding_download_guids.emplace(download.guid);
      continue;
    }

    // Offline pages prefetch uses int64_t for file size. Check for overflow and
    // skip it.
    uint64_t file_size = download.completion_info->bytes_downloaded;
    if (file_size > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
      continue;

    success_downloads.emplace(download.guid,
                              std::make_pair(download.completion_info->path,
                                             static_cast<int64_t>(file_size)));
  }

  PrefetchDownloader* downloader = GetPrefetchDownloader();
  if (downloader) {
    downloader->OnDownloadServiceReady(outstanding_download_guids,
                                       success_downloads);
  }
}

void OfflinePrefetchDownloadClient::OnServiceUnavailable() {
  PrefetchDownloader* downloader = GetPrefetchDownloader();
  if (downloader)
    downloader->OnDownloadServiceUnavailable();
}

void OfflinePrefetchDownloadClient::OnDownloadFailed(
    const std::string& guid,
    const download::CompletionInfo& completion_info,
    download::Client::FailureReason reason) {
  PrefetchDownloader* downloader = GetPrefetchDownloader();
  if (downloader)
    downloader->OnDownloadFailed(guid);
}

void OfflinePrefetchDownloadClient::OnDownloadSucceeded(
    const std::string& guid,
    const download::CompletionInfo& completion_info) {
  // Offline pages prefetch uses int64_t for file size. Check for overflow and
  // skip it.
  uint64_t file_size = completion_info.bytes_downloaded;
  if (file_size > static_cast<uint64_t>(std::numeric_limits<int64_t>::max()))
    return;

  PrefetchDownloader* downloader = GetPrefetchDownloader();
  if (downloader)
    downloader->OnDownloadSucceeded(guid, completion_info.path,
                                    completion_info.bytes_downloaded);
}

bool OfflinePrefetchDownloadClient::CanServiceRemoveDownloadedFile(
    const std::string& guid,
    bool force_delete) {
  return true;
}

void OfflinePrefetchDownloadClient::GetUploadData(
    const std::string& guid,
    download::GetUploadDataCallback callback) {
  base::SequencedTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback), nullptr));
}

PrefetchDownloader* OfflinePrefetchDownloadClient::GetPrefetchDownloader()
    const {
  PrefetchService* prefetch_service =
      PrefetchServiceFactory::GetForKey(simple_factory_key_);
  if (!prefetch_service)
    return nullptr;
  return prefetch_service->GetPrefetchDownloader();
}

}  // namespace offline_pages
