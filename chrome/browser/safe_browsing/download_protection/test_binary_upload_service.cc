// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/download_protection/test_binary_upload_service.h"

#include "base/task/post_task.h"
#include "chrome/browser/safe_browsing/download_protection/binary_fcm_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

TestBinaryUploadService::TestBinaryUploadService()
    : BinaryUploadService(nullptr, std::unique_ptr<BinaryFCMService>(nullptr)) {
}

void TestBinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  base::PostTask(FROM_HERE, {content::BrowserThread::UI},
                 base::BindOnce(&Request::FinishRequest, std::move(request),
                                saved_result_, saved_response_));
  was_called_ = true;
}

void TestBinaryUploadService::SetResponse(Result result,
                                          DeepScanningClientResponse response) {
  saved_result_ = result;
  saved_response_ = response;
}

void TestBinaryUploadService::ClearWasCalled() {
  was_called_ = false;
}

}  // namespace safe_browsing
