// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_NET_NETWORK_FETCHER_TASK_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_NET_NETWORK_FETCHER_TASK_H_

#include <stdint.h>
#include <string>

#include "base/containers/flat_map.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_traits.h"
#include "components/update_client/network.h"

namespace base {
class GURL;
class FilePath;
class SequencedTaskRunner;
}  // namespace base

namespace android_webview {

class NetworkFetcherTask {
 public:
  static std::unique_ptr<NetworkFetcherTask> CreateDownloadToFileTask(
      const GURL& url,
      const base::FilePath& file_path,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback
          download_to_file_complete_callback);
  static std::unique_ptr<NetworkFetcherTask> CreatePostRequestTask(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::PostRequestCompleteCallback
          post_request_complete_callback);

  NetworkFetcherTask(
      const GURL& url,
      const base::FilePath& file_path,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback);

  NetworkFetcherTask(
      const GURL& url,
      const std::string& post_data,
      const std::string& content_type,
      const base::flat_map<std::string, std::string>& post_additional_headers,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::PostRequestCompleteCallback
          post_request_complete_callback);

  ~NetworkFetcherTask();

  void InvokeProgressCallback(int64_t current);
  void InvokeResponseStartedCallback(int response_code, int64_t content_length);
  void InvokeDownloadToFileCompleteCallback(int network_error,
                                            int64_t content_size);
  void InvokePostRequestCompleteCallback(
      std::unique_ptr<std::string> response_body,
      int net_error,
      const std::string& header_etag,
      const std::string& header_x_cup_server_proof,
      int64_t xheader_retry_after_sec);

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::SequencedTaskRunner::GetCurrentDefault();
  update_client::NetworkFetcher::ResponseStartedCallback
      response_started_callback_;
  update_client::NetworkFetcher::ProgressCallback progress_callback_;
  update_client::NetworkFetcher::DownloadToFileCompleteCallback
      download_to_file_complete_callback_;
  update_client::NetworkFetcher::PostRequestCompleteCallback
      post_request_complete_callback_;
  base::WeakPtrFactory<NetworkFetcherTask> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_NET_NETWORK_FETCHER_TASK_H_
