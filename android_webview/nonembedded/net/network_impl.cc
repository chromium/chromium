// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/net/network_impl.h"

#include <memory>
#include <string>
#include <utility>

#include "android_webview/nonembedded/net/network_fetcher_task.h"
#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"

namespace android_webview {

NetworkFetcherFactoryImpl::NetworkFetcherFactoryImpl() = default;

NetworkFetcherFactoryImpl::~NetworkFetcherFactoryImpl() = default;

std::unique_ptr<update_client::NetworkFetcher>
NetworkFetcherFactoryImpl::Create() const {
  return std::make_unique<NetworkFetcherImpl>();
}

NetworkFetcherImpl::NetworkFetcherImpl() = default;

NetworkFetcherImpl::~NetworkFetcherImpl() = default;

void NetworkFetcherImpl::PostRequest(
    const GURL& url,
    const std::string& post_data,
    const std::string& content_type,
    const base::flat_map<std::string, std::string>& post_additional_headers,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    PostRequestCompleteCallback post_request_complete_callback) {
  // A new NetworkFetcherImpl must be created for each network operation.
  DCHECK(!network_task_);

  network_task_ = NetworkFetcherTask::CreatePostRequestTask(
      url, post_data, content_type, post_additional_headers,
      std::move(response_started_callback), progress_callback,
      std::move(post_request_complete_callback));
}

base::OnceClosure NetworkFetcherImpl::DownloadToFile(
    const GURL& url,
    const base::FilePath& file_path,
    ResponseStartedCallback response_started_callback,
    ProgressCallback progress_callback,
    DownloadToFileCompleteCallback download_to_file_complete_callback) {
  // A new NetworkFetcherImpl must be created for each network operation.
  DCHECK(!network_task_);

  network_task_ = NetworkFetcherTask::CreateDownloadToFileTask(
      url, file_path, std::move(response_started_callback), progress_callback,
      std::move(download_to_file_complete_callback));

  return base::DoNothing();
}

}  // namespace android_webview
