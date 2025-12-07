// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/mock_download_controller.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/task/single_thread_task_runner.h"

namespace android {

MockDownloadController::MockDownloadController()
    : approve_file_access_request_(true) {}

MockDownloadController::~MockDownloadController() = default;

void MockDownloadController::OnDownloadStarted(
    download::DownloadItem* download_item) {}

void MockDownloadController::StartContextMenuDownload(
    const GURL& url,
    const content::ContextMenuParams& params,
    content::WebContents* web_contents,
    bool is_media) {}

void MockDownloadController::SetApproveFileAccessRequestForTesting(
    bool approve) {
  approve_file_access_request_ = approve;
}

void MockDownloadController::CreateAndroidDownload(
    const content::WebContents::Getter& wc_getter,
    const DownloadInfo& info) {}
}  // namespace android
