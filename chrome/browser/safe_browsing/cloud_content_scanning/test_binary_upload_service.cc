// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/cloud_content_scanning/test_binary_upload_service.h"

#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_fcm_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace safe_browsing {

TestBinaryUploadService::TestBinaryUploadService() = default;
TestBinaryUploadService::~TestBinaryUploadService() = default;

base::WeakPtr<BinaryUploadService> TestBinaryUploadService::AsWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

void TestBinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  last_request_ = request->content_analysis_request();
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&Request::FinishRequest, std::move(request),
                                saved_result_, saved_response_));
  was_called_ = true;
}

void TestBinaryUploadService::SetResponse(
    Result result,
    enterprise_connectors::ContentAnalysisResponse response) {
  saved_result_ = result;
  saved_response_ = response;
}

void TestBinaryUploadService::ClearWasCalled() {
  was_called_ = false;
}

}  // namespace safe_browsing
