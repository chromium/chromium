// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/nonembedded/net/network_task.h"

#include <utility>

#include "android_webview/nonembedded/net/download_file_task.h"
#include "android_webview/nonembedded/net/network_impl.h"
#include "base/sequenced_task_runner.h"

namespace android_webview {

NetworkTask::NetworkTask(
    update_client::NetworkFetcher::ResponseStartedCallback
        response_started_callback,
    update_client::NetworkFetcher::ProgressCallback progress_callback)
    : response_started_callback_(std::move(response_started_callback)),
      progress_callback_(std::move(progress_callback)) {}

NetworkTask::~NetworkTask() = default;

}  // namespace android_webview
