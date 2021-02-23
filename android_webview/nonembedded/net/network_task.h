// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_NONEMBEDDED_NET_NETWORK_TASK_H_
#define ANDROID_WEBVIEW_NONEMBEDDED_NET_NETWORK_TASK_H_

#include "base/memory/scoped_refptr.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "components/update_client/network.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace android_webview {

class NetworkTask {
 public:
  NetworkTask(
      update_client::NetworkFetcher::ResponseStartedCallback
          response_started_callback,
      update_client::NetworkFetcher::ProgressCallback progress_callback);
  virtual ~NetworkTask();

 protected:
  update_client::NetworkFetcher::ResponseStartedCallback
      response_started_callback_;
  update_client::NetworkFetcher::ProgressCallback progress_callback_;
  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::SequencedTaskRunnerHandle::Get();
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_NONEMBEDDED_NET_NETWORK_TASK_H_
