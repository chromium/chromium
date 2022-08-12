// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service.h"

#include <memory>

#include "base/notreached.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "base/time/time.h"
#include "chrome/browser/enterprise/connectors/analysis/analysis_settings.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/content_analysis_sdk/src/browser/include/content_analysis/sdk/analysis_client.h"

namespace enterprise_connectors {
namespace {

constexpr base::TimeDelta kScanningTimeout = base::Minutes(5);

// Build a content analysis SDK client config based on the request being sent.
content_analysis::sdk::Client::Config SDKConfigFromRequest(
    const safe_browsing::BinaryUploadService::Request* request) {
  return {request->cloud_or_local_settings().local_path(),
          request->cloud_or_local_settings().user_specific()};
}

// Build a content analysis SDK client config based on the ack being sent.
content_analysis::sdk::Client::Config SDKConfigFromAck(
    const safe_browsing::BinaryUploadService::Ack* ack) {
  return {ack->cloud_or_local_settings().local_path(),
          ack->cloud_or_local_settings().user_specific()};
}

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

  // Provide a deadline for the service provider to respond.
  base::Time expires_at = base::Time::Now() + kScanningTimeout;
  request.set_expires_at(expires_at.ToTimeT());

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

content_analysis::sdk::ContentAnalysisAcknowledgement ConvertChromeAckToSDKAck(
    const ContentAnalysisAcknowledgement& ack) {
  content_analysis::sdk::ContentAnalysisAcknowledgement sdk_ack;

  // TODO(b/226679912): Add unit tests to
  // components/enterprise/common/proto/connectors_unittest.cc to ensure the
  // conversion methods here and below always work.
  if (!sdk_ack.ParseFromString(ack.SerializeAsString())) {
    return content_analysis::sdk::ContentAnalysisAcknowledgement();
  }

  return sdk_ack;
}

int SendAckToSDK(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    content_analysis::sdk::ContentAnalysisAcknowledgement sdk_ack) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  int status = -1;
  if (wrapped && wrapped->client())
    status = wrapped->client()->Acknowledge(sdk_ack);

  return status;
}

void HandleAckResponse(
    std::unique_ptr<safe_browsing::BinaryUploadService::Ack> ack,
    int status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status != 0)
    ContentAnalysisSdkManager::Get()->ResetClient(SDKConfigFromAck(ack.get()));
}

void DoSendAck(scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
               std::unique_ptr<safe_browsing::BinaryUploadService::Ack> ack) {
  content_analysis::sdk::ContentAnalysisAcknowledgement sdk_ack =
      ConvertChromeAckToSDKAck(ack->ack());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&SendAckToSDK, wrapped, std::move(sdk_ack)),
      base::BindOnce(&HandleAckResponse, std::move(ack)));
}

// Sends a request to the local server provider and waits for a response.
absl::optional<content_analysis::sdk::ContentAnalysisResponse> SendRequestToSDK(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    content_analysis::sdk::ContentAnalysisRequest sdk_request) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  content_analysis::sdk::ContentAnalysisResponse response;
  if (wrapped && wrapped->client()) {
    int status = wrapped->client()->Send(sdk_request, &response);
    if (status == 0)
      return response;
  }
  return absl::nullopt;
}

// Handles the response from the local service provider.
void HandleResponseFromSDK(
    int64_t expires_at,
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    absl::optional<content_analysis::sdk::ContentAnalysisResponse>
        sdk_response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  safe_browsing::BinaryUploadService::Result result;
  ContentAnalysisResponse response;
  if (base::Time::Now() > base::Time::FromTimeT(expires_at)) {
    result = safe_browsing::BinaryUploadService::Result::TIMEOUT;

    // Send timeout back to service provider.
    auto ack = std::make_unique<safe_browsing::BinaryUploadService::Ack>(
        request->cloud_or_local_settings());
    ack->set_request_token(request->request_token());
    ack->set_status(ContentAnalysisAcknowledgement::TOO_LATE);
    ack->set_final_action(ContentAnalysisAcknowledgement::ALLOW);
    DoSendAck(wrapped, std::move(ack));
  } else if (sdk_response.has_value()) {
    result = safe_browsing::BinaryUploadService::Result::SUCCESS;
    response = ConvertSDKResponseToChromeResponse(sdk_response.value());
  } else {
    result = safe_browsing::BinaryUploadService::Result::UPLOAD_FAILURE;
    ContentAnalysisSdkManager::Get()->ResetClient(
        SDKConfigFromRequest(request.get()));
  }
  request->FinishRequest(result, std::move(response));
}

// Sends a request's data to the local service provider.  Handles the response
// the service provider asynchronously.
void DoLocalContentAnalysis(
    std::unique_ptr<safe_browsing::BinaryUploadService::Request> request,
    safe_browsing::BinaryUploadService::Result result,
    safe_browsing::BinaryUploadService::Request::Data data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (LocalResultIsFailure(result)) {
    request->FinishRequest(result, ContentAnalysisResponse());
    return;
  }

  request->SetRandomRequestToken();
  DCHECK(request->cloud_or_local_settings().is_local_analysis());

  auto client = ContentAnalysisSdkManager::Get()->GetClient(
      SDKConfigFromRequest(request.get()));
  content_analysis::sdk::ContentAnalysisRequest sdk_request =
      ConvertChromeRequestToSDKRequest(request->content_analysis_request());

  if (!data.contents.empty()) {
    sdk_request.set_text_content(std::move(data.contents));
  } else if (!data.path.empty()) {
    sdk_request.set_file_path(data.path.AsUTF8Unsafe());
  } else {
    NOTREACHED();
  }

  auto expires_at = sdk_request.expires_at();

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::SKIP_ON_SHUTDOWN},
      base::BindOnce(&SendRequestToSDK, client, std::move(sdk_request)),
      base::BindOnce(&HandleResponseFromSDK, expires_at, std::move(request),
                     client));
}

}  // namespace

LocalBinaryUploadService::LocalBinaryUploadService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

LocalBinaryUploadService::~LocalBinaryUploadService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void LocalBinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<LocalBinaryUploadService::Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Request* raw_request = request.get();
  raw_request->GetRequestData(
      base::BindOnce(&DoLocalContentAnalysis, std::move(request)));
}

void LocalBinaryUploadService::MaybeAcknowledge(std::unique_ptr<Ack> ack) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Ack* ack_ptr = ack.get();
  DoSendAck(
      ContentAnalysisSdkManager::Get()->GetClient(SDKConfigFromAck(ack_ptr)),
      std::move(ack));
}

}  // namespace enterprise_connectors
