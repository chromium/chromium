// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/mock_download_controller.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/threading/thread_task_runner_handle.h"

namespace chrome {
namespace android {

MockDownloadController::MockDownloadController()
    : approve_file_access_request_(true) {}

MockDownloadController::~MockDownloadController() {}

void MockDownloadController::OnDownloadStarted(
    download::DownloadItem* download_item) {}

void MockDownloadController::StartContextMenuDownload(
    const content::ContextMenuParams& params,
    content::WebContents* web_contents,
    bool is_link,
    const std::string& extra_headers) {}

void MockDownloadController::AcquireFileAccessPermission(
    const content::WebContents::Getter& wc_getter,
    DownloadControllerBase::AcquireFileAccessPermissionCallback cb) {
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::BindOnce(std::move(cb), approve_file_access_request_));
}

void MockDownloadController::SetApproveFileAccessRequestForTesting(
    bool approve) {
  approve_file_access_request_ = approve;
}

void MockDownloadController::CreateAndroidDownload(
    const content::WebContents::Getter& wc_getter,
    const DownloadInfo& info) {}

void MockDownloadController::AboutToResumeDownload(
    download::DownloadItem* download_item) {}

}  // namespace android
}  // namespace chrome
