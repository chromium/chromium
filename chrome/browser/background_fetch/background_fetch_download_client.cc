// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_download_client.h"

#include <memory>
#include <set>
#include <utility>

#include "base/bind.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"
#include "chrome/browser/download/download_service_factory.h"
#include "components/download/public/background_service/download_metadata.h"
#include "components/download/public/background_service/download_service.h"
#include "content/public/browser/background_fetch_response.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "url/origin.h"

namespace {

using BackgroundFetchFailureReason =
    content::BackgroundFetchResult::FailureReason;
BackgroundFetchFailureReason ToBackgroundFetchFailureReason(
    download::Client::FailureReason reason) {
  switch (reason) {
    case download::Client::FailureReason::NETWORK:
      return BackgroundFetchFailureReason::NETWORK;
    case download::Client::FailureReason::UPLOAD_TIMEDOUT:
    case download::Client::FailureReason::TIMEDOUT:
      return BackgroundFetchFailureReason::TIMEDOUT;
    case download::Client::FailureReason::UNKNOWN:
      return BackgroundFetchFailureReason::FETCH_ERROR;
    case download::Client::FailureReason::ABORTED:
    case download::Client::FailureReason::CANCELLED:
      return BackgroundFetchFailureReason::CANCELLED;
  }
}

}  // namespace

BackgroundFetchDownloadClient::BackgroundFetchDownloadClient(
    content::BrowserContext* context)
    : browser_context_(context), delegate_(nullptr) {}

BackgroundFetchDownloadClient::~BackgroundFetchDownloadClient() = default;

void BackgroundFetchDownloadClient::OnServiceInitialized(
    bool state_lost,
    const std::vector<download::DownloadMetaData>& downloads) {
  std::set<std::string> outstanding_guids =
      GetDelegate()->TakeOutstandingGuids();
  for (const auto& download : downloads) {
    if (!outstanding_guids.count(download.guid)) {
      // Background Fetch is not aware of this GUID, so it successfully
      // completed but the information is still around.
      continue;
    }

    if (download.completion_info) {
      // The download finished but was not persisted.
      OnDownloadSucceeded(download.guid, *download.completion_info);
      return;
    }

    // The download is active, and will call the appropriate functions.

    if (download.paused) {
      // We need to resurface the notification in a paused state.
      content::BrowserThread::PostBestEffortTask(
          FROM_HERE, base::SequencedTaskRunnerHandle::Get(),
          base::BindOnce(&BackgroundFetchDelegateImpl::RestartPausedDownload,
                         GetDelegate()->GetWeakPtr(), download.guid));
    }
  }

  // There is also the case that the Download Service is not aware of the GUID.
  // i.e. there is a guid in |outstanding_guids| not in |downloads|.
  // This can be due to:
  // 1. The browser crashing before the download started.
  // 2. The download failing before persisting the state.
  // 3. The browser was forced to clean-up the the download.
  // In either case the download should be allowed to restart, so there is
  // nothing to do here.
}

void BackgroundFetchDownloadClient::OnServiceUnavailable() {}

void BackgroundFetchDownloadClient::OnDownloadStarted(
    const std::string& guid,
    const std::vector<GURL>& url_chain,
    const scoped_refptr<const net::HttpResponseHeaders>& headers) {
  // TODO(crbug.com/884672): Validate the chain/headers and cancel the download
  // if invalid.
  auto response =
      std::make_unique<content::BackgroundFetchResponse>(url_chain, headers);
  GetDelegate()->OnDownloadStarted(guid, std::move(response));
}

void BackgroundFetchDownloadClient::OnDownloadUpdated(
    const std::string& guid,
    uint64_t bytes_uploaded,
    uint64_t bytes_downloaded) {
  GetDelegate()->OnDownloadUpdated(guid, bytes_uploaded, bytes_downloaded);
}

void BackgroundFetchDownloadClient::OnDownloadFailed(
    const std::string& guid,
    const download::CompletionInfo& info,
    download::Client::FailureReason reason) {
  auto response = std::make_unique<content::BackgroundFetchResponse>(
      info.url_chain, info.response_headers);
  auto result = std::make_unique<content::BackgroundFetchResult>(
      std::move(response), base::Time::Now(),
      ToBackgroundFetchFailureReason(reason));
  GetDelegate()->OnDownloadFailed(guid, std::move(result));
}

void BackgroundFetchDownloadClient::OnDownloadSucceeded(
    const std::string& guid,
    const download::CompletionInfo& info) {
  if (browser_context_->IsOffTheRecord())
    DCHECK(info.blob_handle);
  else
    DCHECK(!info.path.empty());

  auto response = std::make_unique<content::BackgroundFetchResponse>(
      info.url_chain, info.response_headers);
  auto result = std::make_unique<content::BackgroundFetchResult>(
      std::move(response), base::Time::Now(), info.path, info.blob_handle,
      info.bytes_downloaded);

  GetDelegate()->OnDownloadSucceeded(guid, std::move(result));
}

bool BackgroundFetchDownloadClient::CanServiceRemoveDownloadedFile(
    const std::string& guid,
    bool force_delete) {
  // If |force_delete| is true the file will be removed anyway.
  // TODO(rayankans): Add UMA to see how often this happens.
  return force_delete || GetDelegate()->IsGuidOutstanding(guid);
}

void BackgroundFetchDownloadClient::GetUploadData(
    const std::string& guid,
    download::GetUploadDataCallback callback) {
  GetDelegate()->GetUploadData(guid, std::move(callback));
}

BackgroundFetchDelegateImpl* BackgroundFetchDownloadClient::GetDelegate() {
  if (delegate_)
    return delegate_.get();

  content::BackgroundFetchDelegate* delegate =
      browser_context_->GetBackgroundFetchDelegate();

  delegate_ = static_cast<BackgroundFetchDelegateImpl*>(delegate)->GetWeakPtr();
  DCHECK(delegate_);
  return delegate_.get();
}
