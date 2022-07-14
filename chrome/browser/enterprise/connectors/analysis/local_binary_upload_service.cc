// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service.h"

#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/enterprise/connectors/analysis/analysis_settings.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/content_analysis_sdk/src/browser/include/content_analysis/sdk/analysis_client.h"

namespace enterprise_connectors {
namespace {

// Convert enterprise connector ContentAnalysisRequest into the SDK equivalent.
// SDK ContentAnalysisRequest is a strict subset of the enterprise connector
// version, therefore the function should always work.
content_analysis::sdk::ContentAnalysisRequest ConvertChromeRequestToSDKRequest(
    const ContentAnalysisRequest& req) {
  content_analysis::sdk::ContentAnalysisRequest request;

  // TODO(b/226679912): Add unit tests to
  // components/enterprise/common/proto/connectors_unittest.cc to ensure the
  // conversion methods here and below always work.
  if (!request.ParseFromString(req.SerializeAsString())) {
    return content_analysis::sdk::ContentAnalysisRequest();
  }

  return request;
}

// Convert SDK ContentAnalysisResponse into the enterprise connector equivalent.
// SDK ContentAnalysisResponse is a strict subset of the enterprise connector
// version, therefore the function should always work.
ContentAnalysisResponse ConvertSDKResponseToChromeResponse(
    const content_analysis::sdk::ContentAnalysisResponse& res) {
  ContentAnalysisResponse response;

  if (!response.ParseFromString(res.SerializeAsString())) {
    return ContentAnalysisResponse();
  }

  return response;
}

absl::optional<content_analysis::sdk::ContentAnalysisResponse> SendRequestToSDK(
    content_analysis::sdk::Client* client,
    content_analysis::sdk::ContentAnalysisRequest
        local_content_analysis_request) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  content_analysis::sdk::ContentAnalysisResponse response;
  client->Send(local_content_analysis_request, &response);

  return response;
}

}  // namespace

LocalBinaryUploadService::LocalBinaryUploadService(
    std::unique_ptr<AnalysisSettings> analysis_settings)
    : analysis_settings_(std::move(analysis_settings)) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

LocalBinaryUploadService::~LocalBinaryUploadService() = default;

void LocalBinaryUploadService::OnSentRequestStatus(
    std::unique_ptr<Request> request,
    absl::optional<content_analysis::sdk::ContentAnalysisResponse> response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Result result;
  ContentAnalysisResponse chrome_content_analysis_response;
  if (response.has_value()) {
    result = Result::SUCCESS;
    chrome_content_analysis_response =
        ConvertSDKResponseToChromeResponse(response.value());
  } else {
    result = Result::UPLOAD_FAILURE;
    // Release old client when the status is not ok.
    client_.reset();
  }
  request->FinishRequest(result, std::move(chrome_content_analysis_response));
}

void LocalBinaryUploadService::DoLocalContentAnalysis(
    std::unique_ptr<Request> request,
    Result result,
    Request::Data data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (result != Result::SUCCESS) {
    request->FinishRequest(result, ContentAnalysisResponse());
    return;
  }

  // TODO(b/226679912): Add logic to support OS-user-specific agents.
  if (!client_) {
    DCHECK(analysis_settings_->cloud_or_local_settings.is_local_analysis());
    client_ = content_analysis::sdk::Client::Create(
        {analysis_settings_->cloud_or_local_settings.local_settings()
             .local_path});
  }
  content_analysis::sdk::ContentAnalysisRequest local_content_analysis_request =
      ConvertChromeRequestToSDKRequest(request->content_analysis_request());

  if (!data.contents.empty()) {
    local_content_analysis_request.set_text_content(std::move(data.contents));
  } else if (!data.path.empty()) {
    local_content_analysis_request.set_file_path(data.path.AsUTF8Unsafe());
  } else {
    NOTREACHED();
  }

  // TODO(b/238897238): Manage SDK client pointer via
  // ChromeBrowserPolicyConnector.
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      // `client_` passed as naked pointer to avoid using `std::move()` which
      // will reset `client_`, making chrome continually connecting and
      // disconnecting from agent.
      // Because LocalBinaryUploadService is a profile keyed service, the object
      // should live for at least as long as the profile. Besides, the task is
      // posted with `SKIP_ON_SHUTDOWN`, therefore if the task is pending on
      // shutdown, it won't run.
      base::BindOnce(&SendRequestToSDK, base::Unretained(client_.get()),
                     std::move(local_content_analysis_request)),
      base::BindOnce(&LocalBinaryUploadService::OnSentRequestStatus,
                     weakptr_factory_.GetWeakPtr(), std::move(request)));
}

void LocalBinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<LocalBinaryUploadService::Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Request* raw_request = request.get();
  raw_request->GetRequestData(
      base::BindOnce(&LocalBinaryUploadService::DoLocalContentAnalysis,
                     weakptr_factory_.GetWeakPtr(), std::move(request)));
}

}  // namespace enterprise_connectors
