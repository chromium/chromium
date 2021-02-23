// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_NET_DOWNLOAD_FILE_TASK_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_NET_DOWNLOAD_FILE_TASK_H_

#include <stdint.h>

#include "android_webview/nonembedded/net/network_task.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/task/task_traits.h"

namespace base {
class GURL;
class FilePath;
}  // namespace base

namespace android_webview {

class DownloadFileTask : public NetworkTask {
 public:
  DownloadFileTask(
      const GURL& url,
      const base::FilePath& file_path,
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback,
      update_client::NetworkFetcher::DownloadToFileCompleteCallback
          download_to_file_complete_callback);
  ~DownloadFileTask() override;

  void InvokeProgressCallback(int64_t current);
  void InvokeResponseStartedCallback(int response_code, int64_t content_length);
  void InvokeDownloadToFileCompleteCallback(int network_error,
                                            int64_t content_size);

 private:
  SEQUENCE_CHECKER(sequence_checker_);
  update_client::NetworkFetcher::DownloadToFileCompleteCallback
      download_to_file_complete_callback_;
  base::WeakPtrFactory<DownloadFileTask> weak_ptr_factory_{this};
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_NET_DOWNLOAD_FILE_TASK_H_
