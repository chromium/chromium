// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_NET_NETWORK_IMPL_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_NET_NETWORK_IMPL_H_

#include <memory>

#include "android_webview/nonembedded/net/network_fetcher_task.h"
#include "base/functional/callback_forward.h"
#include "components/update_client/network.h"

namespace android_webview {

class NetworkFetcherFactoryImpl : public update_client::NetworkFetcherFactory {
 public:
  NetworkFetcherFactoryImpl();
  NetworkFetcherFactoryImpl(const NetworkFetcherFactoryImpl&) = delete;
  NetworkFetcherFactoryImpl& operator=(const NetworkFetcherFactoryImpl&) =
      delete;

  std::unique_ptr<update_client::NetworkFetcher> Create() const override;

 protected:
  ~NetworkFetcherFactoryImpl() override;
};

class NetworkFetcherImpl : public update_client::NetworkFetcher {
 public:
  NetworkFetcherImpl();
  ~NetworkFetcherImpl() override;
  NetworkFetcherImpl(const NetworkFetcherImpl&) = delete;
  NetworkFetcherImpl& operator=(const NetworkFetcherImpl&) = delete;

  // NetworkFetcher overrides.
  void PostRequest(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      PostRequestCompleteCallback post_request_complete_callback) override;
  base::OnceClosure DownloadToFile(
      const GURL& url,
      const base::FilePath& file_path,
      ResponseStartedCallback response_started_callback,
      ProgressCallback progress_callback,
      DownloadToFileCompleteCallback download_to_file_complete_callback)
      override;

 private:
  std::unique_ptr<NetworkFetcherTask> network_task_;
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_NET_NETWORK_IMPL_H_
