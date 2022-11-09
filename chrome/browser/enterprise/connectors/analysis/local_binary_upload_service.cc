// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service.h"

#include <algorithm>
#include <memory>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/enterprise/connectors/analysis/analysis_settings.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/content_analysis_sdk/src/browser/include/content_analysis/sdk/analysis_client.h"

namespace enterprise_connectors {
namespace {

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

// Build a content analysis SDK client config based on the cancel requests being
// sent.
content_analysis::sdk::Client::Config SDKConfigFromCancel(
    const safe_browsing::BinaryUploadService::CancelRequests* cancel) {
  return {cancel->cloud_or_local_settings().local_path(),
          cancel->cloud_or_local_settings().user_specific()};
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
  base::Time expires_at =
      base::Time::Now() + LocalBinaryUploadService::kScanningTimeout;
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
  return wrapped->client()->Acknowledge(sdk_ack);
}

int SendCancelToSDK(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    content_analysis::sdk::ContentAnalysisCancelRequests sdk_cancel) {
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);
  return wrapped->client()->CancelRequests(sdk_cancel);
}

void HandleAckOrCancelResponse(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    int status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status != 0) {
    ContentAnalysisSdkManager::Get()->ResetClient(
        wrapped->client()->GetConfig());
  }
}

void DoSendAck(scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
               std::unique_ptr<safe_browsing::BinaryUploadService::Ack> ack) {
  if (!wrapped || !wrapped->client())
    return;

  content_analysis::sdk::ContentAnalysisAcknowledgement sdk_ack =
      ConvertChromeAckToSDKAck(ack->ack());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SendAckToSDK, wrapped, std::move(sdk_ack)),
      base::BindOnce(&HandleAckOrCancelResponse, wrapped));
}

void DoSendCancel(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    std::unique_ptr<safe_browsing::BinaryUploadService::CancelRequests>
        cancel) {
  if (!wrapped || !wrapped->client())
    return;

  content_analysis::sdk::ContentAnalysisCancelRequests sdk_cancel;
  sdk_cancel.set_user_action_id(cancel->get_user_action_id());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SendCancelToSDK, wrapped, std::move(sdk_cancel)),
      base::BindOnce(&HandleAckOrCancelResponse, wrapped));
}

// Sends a request to the local agent and waits for a response.
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

#if defined(_DEBUG)
void DumpSdkAnalysisResponse(
    const char* prefix,
    LocalBinaryUploadService::RequestKey key,
    const content_analysis::sdk::ContentAnalysisResponse& response) {
  DVLOG(1) << prefix << " key=" << key << " token=" << response.request_token();
  DVLOG(1) << prefix << " key=" << key
           << " result count=" << response.results().size();

  for (const auto& result : response.results()) {
    if (result.has_status()) {
      DVLOG(1) << prefix << " key=" << key
               << "   result status=" << result.status();
    } else {
      DVLOG(1) << prefix << " key=" << key << "   result status=<no status>";
    }

    if (!result.has_status() ||
        result.status() !=
            content_analysis::sdk::ContentAnalysisResponse::Result::SUCCESS) {
      continue;
    }

    DVLOG(1) << prefix << " key=" << key
             << "   rules count=" << result.triggered_rules().size();

    for (const auto& rule : result.triggered_rules()) {
      DVLOG(1) << prefix << " key=" << key
               << "     rule action=" << rule.action()
               << " tag=" << result.tag();
    }
  }
}

void DumpAnalysisResponse(const char* prefix,
                          LocalBinaryUploadService::RequestKey key,
                          const ContentAnalysisResponse& response) {
  auto final_action = TriggeredRule::ACTION_UNSPECIFIED;
  std::string tag;

  DVLOG(1) << prefix << " key=" << key << " token=" << response.request_token();
  DVLOG(1) << prefix << " key=" << key
           << " result count=" << response.results().size();

  for (const auto& result : response.results()) {
    if (result.has_status()) {
      DVLOG(1) << prefix << " key=" << key
               << "   result status=" << result.status();
    } else {
      DVLOG(1) << prefix << " key=" << key << "   result status=<no status>";
    }

    if (!result.has_status() ||
        result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      continue;
    }

    DVLOG(1) << prefix << " key=" << key
             << "   rules count=" << result.triggered_rules().size();

    for (const auto& rule : result.triggered_rules()) {
      auto higher_precedence_action =
          GetHighestPrecedenceAction(final_action, rule.action());
      DVLOG(1) << prefix << " key=" << key
               << "     rule action=" << rule.action()
               << " tag=" << result.tag();

      if (higher_precedence_action != final_action) {
        tag = result.tag();
      }
      final_action = higher_precedence_action;
    }
  }

  DVLOG(1) << prefix << " key=" << key << " final action=" << final_action;
}
#endif

}  // namespace

LocalBinaryUploadService::RequestInfo::RequestInfo(
    std::unique_ptr<LocalBinaryUploadService::Request> request,
    base::OnceClosure closure)
    : request(std::move(request)) {
  started_at = base::TimeTicks::Now();
  timer = std::make_unique<base::OneShotTimer>();
  timer->Start(FROM_HERE, kScanningTimeout, std::move(closure));
}

LocalBinaryUploadService::RequestInfo::RequestInfo(
    RequestInfo&& other) noexcept = default;

LocalBinaryUploadService::RequestInfo&
LocalBinaryUploadService::RequestInfo::operator=(RequestInfo&& other) noexcept =
    default;

LocalBinaryUploadService::RequestInfo::~RequestInfo() noexcept = default;

LocalBinaryUploadService::LocalBinaryUploadService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

LocalBinaryUploadService::~LocalBinaryUploadService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void LocalBinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // If there have been too may consecutive failures accessing the agent,
  // just fail the request immediately.  Chrome will apply the default verdict.
  if (retry_count_ >= kMaxRetryCount) {
    DVLOG(1) << "MaybeUploadForDeepScanning aborting, too many errors";
    request->FinishRequest(Result::UPLOAD_FAILURE, ContentAnalysisResponse());
    return;
  }

  // Builds a request context to keep track of this request.  This starts
  // a timer that will fire if no response is received from the agent within
  // the specified timeout.  This timer remains active as the request moves
  // from the pending list to the active list (and possibly back and forth in
  // the case of agent errors).
  RequestKey key = request.get();
  auto info = RequestInfo(std::move(request),
                          base::BindOnce(&LocalBinaryUploadService::OnTimeout,
                                         factory_.GetWeakPtr(), key));
  DVLOG(1) << "MaybeUploadForDeepScanning key=" << key
           << " active-size=" << active_requests_.size();
  if (active_requests_.size() < kMaxActiveCount) {
    active_requests_.emplace(key, std::move(info));
    ProcessRequest(key);
  } else {
    DVLOG(1) << "MaybeUploadForDeepScanning key=" << key
             << " adding to pending queue";
    pending_requests_.push_back(std::move(info));
  }
}

void LocalBinaryUploadService::MaybeAcknowledge(std::unique_ptr<Ack> ack) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  Ack* ack_ptr = ack.get();
  DoSendAck(
      ContentAnalysisSdkManager::Get()->GetClient(SDKConfigFromAck(ack_ptr)),
      std::move(ack));
}

void LocalBinaryUploadService::MaybeCancelRequests(
    std::unique_ptr<CancelRequests> cancel) {
  // Cancel all active requests.  If the agent returns a response for any,
  // they will be ignored.
  for (auto it = active_requests_.begin(); it != active_requests_.end();) {
    if (it->second.request->user_action_id() == cancel->get_user_action_id()) {
      it = active_requests_.erase(it);
    } else {
      ++it;
    }
  }

  // Cancel all pending requests.
  for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
    if (it->request->user_action_id() == cancel->get_user_action_id()) {
      it = pending_requests_.erase(it);
    } else {
      ++it;
    }
  }

  // Tell agent to cancel requests.  This is a best effort only on the part of
  // the agent.
  auto* cancel_ptr = cancel.get();
  DoSendCancel(ContentAnalysisSdkManager::Get()->GetClient(
                   SDKConfigFromCancel(cancel_ptr)),
               std::move(cancel));
}

void LocalBinaryUploadService::DoLocalContentAnalysis(RequestKey key,
                                                      Result result,
                                                      Request::Data data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "DoLocalContentAnalysis key=" << key;

  if (LocalResultIsFailure(result)) {
    FinishRequest(key, result, ContentAnalysisResponse());
    return;
  }

  auto it = active_requests_.find(key);
  if (it == active_requests_.end())
    return;

  const auto& info = it->second;

  // If this is a retry, the request token is already set.  Don't set
  // it again.
  if (info.request->request_token().empty()) {
    info.request->SetRandomRequestToken();
    DVLOG(1) << "DoLocalContentAnalysis key=" << key
             << " new request_token=" << info.request->request_token();
  } else {
    DVLOG(1) << "DoLocalContentAnalysis key=" << key
             << " existing request_token=" << info.request->request_token();
  }

  DCHECK(info.request->cloud_or_local_settings().is_local_analysis());

  auto wrapped = ContentAnalysisSdkManager::Get()->GetClient(
      SDKConfigFromRequest(info.request.get()));
  if (!wrapped || !wrapped->client()) {
    FinishRequest(key, Result::UPLOAD_FAILURE, ContentAnalysisResponse());
    return;
  }

  content_analysis::sdk::ContentAnalysisRequest sdk_request =
      ConvertChromeRequestToSDKRequest(
          info.request->content_analysis_request());

  if (!data.contents.empty()) {
    sdk_request.set_text_content(std::move(data.contents));
  } else if (!data.path.empty()) {
    sdk_request.set_file_path(data.path.AsUTF8Unsafe());
    DVLOG(1) << "DoLocalContentAnalysis key=" << key
             << " file=" << data.path.AsUTF8Unsafe();
  } else if (data.page.IsValid()) {
    auto mapping = data.page.Map();
    sdk_request.mutable_text_content()->assign(mapping.GetMemoryAs<char>(),
                                               mapping.size());
  } else {
    NOTREACHED();
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SendRequestToSDK, wrapped, std::move(sdk_request)),
      base::BindOnce(&LocalBinaryUploadService::HandleResponse,
                     factory_.GetWeakPtr(), wrapped));
}

void LocalBinaryUploadService::HandleResponse(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    absl::optional<content_analysis::sdk::ContentAnalysisResponse>
        sdk_response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (!sdk_response.has_value()) {
    DVLOG(1) << "HandleResponse reset client";
    // An error occurred trying to send to agent.  Reset the client so that the
    // next request attempts to reconnect to the agent.
    ContentAnalysisSdkManager::Get()->ResetClient(
        wrapped->client()->GetConfig());

    // Put the request into the pending queue.  Queue up a call to retry
    // connecting to the agent in order to start processing requests again.
    RetryActiveRequestsSoonOrFailAllRequests();
    return;
  }

  retry_count_ = 0;

  // Find the request that corresponds to this response.  It's possible the
  // request is not found if for example it was cancelled by the user or it
  // timed out.
  RequestKey key = FindRequestByToken(sdk_response.value());
  if (key != nullptr) {
#if defined(_DEBUG)
    DumpSdkAnalysisResponse("HandleResponse", key, sdk_response.value());
#endif

    auto response = ConvertSDKResponseToChromeResponse(sdk_response.value());
    FinishRequest(key, Result::SUCCESS, std::move(response));
    ProcessNextPendingRequest();
  }
}

LocalBinaryUploadService::RequestKey
LocalBinaryUploadService::FindRequestByToken(
    const content_analysis::sdk::ContentAnalysisResponse& sdk_response) {
  // Request must be currently active.
  const auto& request_token = sdk_response.request_token();
  auto it = std::find_if(active_requests_.begin(), active_requests_.end(),
                         [request_token](const auto& value_type) {
                           return request_token ==
                                  value_type.second.request->request_token();
                         });
  return it != active_requests_.end() ? it->first : nullptr;
}

void LocalBinaryUploadService::ProcessNextPendingRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (pending_requests_.size() > 0) {
    auto info = std::move(pending_requests_.front());
    pending_requests_.erase(pending_requests_.begin());

    RequestKey key = info.request.get();
    active_requests_.emplace(key, std::move(info));
    ProcessRequest(key);
  }
}

void LocalBinaryUploadService::ProcessRequest(RequestKey key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "ProcessRequest key=" << key;
  DCHECK_GT(active_requests_.count(key), 0u);
  const auto& info = active_requests_.at(key);
  info.request->StartRequest();
  info.request->GetRequestData(
      base::BindOnce(&LocalBinaryUploadService::DoLocalContentAnalysis,
                     factory_.GetWeakPtr(), key));
}

void LocalBinaryUploadService::FinishRequest(RequestKey key,
                                             Result result,
                                             ContentAnalysisResponse response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if defined(_DEBUG)
  DumpAnalysisResponse("FinishRequest", key, response);
#endif

  auto it = active_requests_.find(key);
  if (it != active_requests_.end()) {
    const auto& info = it->second;
    RecordRequestMetrics(info, result, response);
    info.request->FinishRequest(result, std::move(response));
    active_requests_.erase(key);
  } else {
    DVLOG(1) << "FinishRequest key=" << key << " not active";
  }

  auto it2 = base::ranges::find(
      pending_requests_, key,
      [](const RequestInfo& info) { return info.request.get(); });
  if (it2 != pending_requests_.end())
    pending_requests_.erase(it2);
}

void LocalBinaryUploadService::OnTimeout(RequestKey key) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "OnTimeout key=" << key;

  if (active_requests_.count(key) > 0) {
    const auto& info = active_requests_.at(key);
    RecordRequestMetrics(info, Result::TIMEOUT, ContentAnalysisResponse());

    std::unique_ptr<Ack> ack =
        std::make_unique<Ack>(info.request->cloud_or_local_settings());
    ack->set_request_token(info.request->request_token());
    ack->set_status(
        enterprise_connectors::ContentAnalysisAcknowledgement::TOO_LATE);
    DoSendAck(ContentAnalysisSdkManager::Get()->GetClient(
                  SDKConfigFromRequest(info.request.get())),
              std::move(ack));
  }

  FinishRequest(key, BinaryUploadService::Result::TIMEOUT,
                ContentAnalysisResponse());
  ProcessNextPendingRequest();
}

void LocalBinaryUploadService::RetryActiveRequestsSoonOrFailAllRequests() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "RetryActiveRequestsSoonOrFailAllRequests current-retry-count="
           << retry_count_;

  // True if requests should be marked as failed.  Otherwise active requests
  // should be moved to the pending list.
  bool fail_requests = retry_count_ >= kMaxRetryCount;

  // Process active list.
  for (auto it = active_requests_.begin(); it != active_requests_.end();
       it = active_requests_.begin()) {
    if (fail_requests) {
      FinishRequest(it->second.request.get(),
                    BinaryUploadService::Result::UPLOAD_FAILURE,
                    ContentAnalysisResponse());
      continue;
    }

    auto info = std::move(it->second);
    active_requests_.erase(it);
    pending_requests_.push_back(std::move(info));
  }

  // If there have been too many errors, fail all requests on the pending
  // list.  Otherwise attempt to reconnect to the agent and begin processing
  // requests again.
  if (fail_requests) {
    for (auto it = pending_requests_.begin(); it != pending_requests_.end();
         it = pending_requests_.begin()) {
      FinishRequest(it->request.get(),
                    BinaryUploadService::Result::UPLOAD_FAILURE,
                    ContentAnalysisResponse());
    }
  } else {
    if (!connection_retry_timer_.IsRunning()) {
      ++retry_count_;
      connection_retry_timer_.Start(
          FROM_HERE, retry_count_ * base::Seconds(1),
          base::BindOnce(&LocalBinaryUploadService::OnConnectionRetry,
                         factory_.GetWeakPtr()));
    }
  }
}

void LocalBinaryUploadService::OnConnectionRetry() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << "OnConnectionRetry";

  // Move as many requests from the pending list to the active list until the
  // maximum has been reached.
  while (active_requests_.size() < LocalBinaryUploadService::kMaxActiveCount &&
         !pending_requests_.empty()) {
    ProcessNextPendingRequest();
  }
}

void LocalBinaryUploadService::RecordRequestMetrics(
    const RequestInfo& info,
    Result result,
    const enterprise_connectors::ContentAnalysisResponse& response) {
  base::UmaHistogramEnumeration("SafeBrowsing.LocalBinaryUploadRequest.Result",
                                result);
  base::UmaHistogramCustomTimes(
      "SafeBrowsing.LocalBinaryUploadRequest.Duration",
      base::TimeTicks::Now() - info.started_at, base::Milliseconds(1),
      base::Minutes(6), 50);

  for (const auto& response_result : response.results()) {
    if (response_result.tag() == "dlp") {
      base::UmaHistogramBoolean(
          "SafeBrowsing.LocalBinaryUploadRequest.DlpResult",
          response_result.status() !=
              enterprise_connectors::ContentAnalysisResponse::Result::FAILURE);
    }
  }
}

}  // namespace enterprise_connectors
