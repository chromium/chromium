// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/local_binary_upload_service.h"

#include <algorithm>
#include <memory>
#include <optional>

#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/ranges/algorithm.h"
#include "base/syslog_logging.h"
#include "base/task/thread_pool.h"
#include "base/threading/scoped_blocking_call.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/enterprise/signals/system_signals_service_host_factory.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"
#include "components/device_signals/core/browser/system_signals_service_host.h"
#include "components/enterprise/connectors/core/analysis_settings.h"
#include "content/public/browser/browser_thread.h"
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

// Sends a request to the local agent and waits for a response.
std::optional<content_analysis::sdk::ContentAnalysisResponse> SendRequestToSDK(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    content_analysis::sdk::ContentAnalysisRequest sdk_request) {
  DVLOG(1) << __func__ << ": token=" << sdk_request.request_token();
  base::ScopedBlockingCall scoped_blocking_call(FROM_HERE,
                                                base::BlockingType::MAY_BLOCK);

  content_analysis::sdk::ContentAnalysisResponse response;
  if (wrapped && wrapped->client()) {
    int status = wrapped->client()->Send(std::move(sdk_request), &response);
    if (status == 0)
      return response;
  }
  return std::nullopt;
}

#if defined(_DEBUG)
void DumpSdkAnalysisResponse(
    const char* prefix,
    LocalBinaryUploadService::Request::Id id,
    const content_analysis::sdk::ContentAnalysisResponse& response) {
  DVLOG(1) << prefix << " id=" << id << " token=" << response.request_token();
  DVLOG(1) << prefix << " id=" << id
           << " result count=" << response.results().size();

  for (const auto& result : response.results()) {
    if (result.has_status()) {
      DVLOG(1) << prefix << " id=" << id
               << "   result status=" << result.status();
    } else {
      DVLOG(1) << prefix << " id=" << id << "   result status=<no status>";
    }

    if (!result.has_status() ||
        result.status() !=
            content_analysis::sdk::ContentAnalysisResponse::Result::SUCCESS) {
      continue;
    }

    DVLOG(1) << prefix << " id=" << id
             << "   rules count=" << result.triggered_rules().size();

    for (const auto& rule : result.triggered_rules()) {
      DVLOG(1) << prefix << " id=" << id << "     rule action=" << rule.action()
               << " tag=" << result.tag();
    }
  }
}

void DumpAnalysisResponse(const char* prefix,
                          LocalBinaryUploadService::Request::Id id,
                          const ContentAnalysisResponse& response) {
  auto final_action = TriggeredRule::ACTION_UNSPECIFIED;
  std::string tag;

  DVLOG(1) << prefix << " id=" << id << " token=" << response.request_token();
  DVLOG(1) << prefix << " id=" << id
           << " result count=" << response.results().size();

  for (const auto& result : response.results()) {
    if (result.has_status()) {
      DVLOG(1) << prefix << " id=" << id
               << "   result status=" << result.status();
    } else {
      DVLOG(1) << prefix << " id=" << id << "   result status=<no status>";
    }

    if (!result.has_status() ||
        result.status() != ContentAnalysisResponse::Result::SUCCESS) {
      continue;
    }

    DVLOG(1) << prefix << " id=" << id
             << "   rules count=" << result.triggered_rules().size();

    for (const auto& rule : result.triggered_rules()) {
      auto higher_precedence_action =
          GetHighestPrecedenceAction(final_action, rule.action());
      DVLOG(1) << prefix << " id=" << id << "     rule action=" << rule.action()
               << " tag=" << result.tag();

      if (higher_precedence_action != final_action) {
        tag = result.tag();
      }
      final_action = higher_precedence_action;
    }
  }

  DVLOG(1) << prefix << " id=" << id << " final action=" << final_action;
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

LocalBinaryUploadService::LocalBinaryUploadService(Profile* profile)
    : profile_(profile) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

LocalBinaryUploadService::~LocalBinaryUploadService() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
}

void LocalBinaryUploadService::MaybeUploadForDeepScanning(
    std::unique_ptr<Request> request) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Builds a request context to keep track of this request.  This starts
  // a timer that will fire if no response is received from the agent within
  // the specified timeout.  This timer remains active as the request moves
  // from the pending list to the active list (and possibly back and forth in
  // the case of agent errors).
  Request::Id id = request_id_generator_.GenerateNextId();
  request->set_id(id);
  auto info = RequestInfo(std::move(request),
                          base::BindOnce(&LocalBinaryUploadService::OnTimeout,
                                         factory_.GetWeakPtr(), id));
  pending_requests_.push_back(std::move(info));

  bool connection_retry_in_progress = ConnectionRetryInProgress();

  DVLOG(1) << __func__ << ": id=" << id
           << " active-size=" << active_requests_.size()
           << " retry-in-prog=" << connection_retry_in_progress;

  // If the active list is not full and the service is not in the process
  // of reconnecting to the agent, process it now.
  if (active_requests_.size() < kMaxActiveCount &&
      !connection_retry_in_progress) {
    ProcessNextPendingRequest();
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
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // Cancel all pending requests.
  for (auto it = pending_requests_.begin(); it != pending_requests_.end();) {
    if (it->request->user_action_id() == cancel->get_user_action_id()) {
      // This does not calls the `FinishRequest()` method because it would
      // invalidate the iterator.
      it->request->FinishRequest(Result::UPLOAD_FAILURE,
                                 ContentAnalysisResponse());
      it = pending_requests_.erase(it);
    } else {
      ++it;
    }
  }

  pending_cancel_requests_.insert(std::move(cancel));
  SendCancelRequestsIfNeeded();
}

base::WeakPtr<safe_browsing::BinaryUploadService>
LocalBinaryUploadService::AsWeakPtr() {
  return factory_.GetWeakPtr();
}

device_signals::mojom::SystemSignalsService*
LocalBinaryUploadService::GetSystemSignalsService() {
  auto* host =
      enterprise_signals::SystemSignalsServiceHostFactory::GetForProfile(
          profile_);
  return host ? host->GetService() : nullptr;
}

void LocalBinaryUploadService::StartAgentVerification(
    const content_analysis::sdk::Client::Config& config,
    base::span<const char* const> subject_names) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << __func__;

  // If the service is not available, fail open.
  auto* service = GetSystemSignalsService();
  if (!service) {
    DVLOG(1) << __func__ << ": SystemSignalsServiceHost not avaiable";
    OnFileSystemSignals(config, base::span<const char* const>(), {});
    return;
  }

  auto wrapped = ContentAnalysisSdkManager::Get()->GetClient(config);
  if (!wrapped) {
    OnFileSystemSignals(config, subject_names, {});
    return;
  }

  // If the agent is already in the process of being verified, just wait.
  if (IsAgentBeingVerified(config)) {
    return;
  }

  is_agent_verified_[config] = false;

  device_signals::GetFileSystemInfoOptions options;
  options.file_path = base::FilePath::FromUTF8Unsafe(
      wrapped->client()->GetAgentInfo().binary_path);
  options.compute_executable_metadata = true;
  options.compute_sha256 = false;
  service->GetFileSystemSignals(
      {options}, base::BindOnce(&LocalBinaryUploadService::OnFileSystemSignals,
                                factory_.GetWeakPtr(), config, subject_names));
}

void LocalBinaryUploadService::OnFileSystemSignals(
    content_analysis::sdk::Client::Config config,
    base::span<const char* const> subject_names,
    const std::vector<device_signals::FileSystemItem>& items) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << __func__;

  bool agent_is_verified = false;

  // If subject names are specified, make sure the executable is properly signed
  // from ths OSes point of view and it's subject name is found in the provided
  // list.

  if (subject_names.size() > 0) {
    // Make sure we have information about the binary and that the OS has
    // verified its signature.
    if (items.size() > 0 && items[0].executable_metadata &&
        items[0].executable_metadata->is_os_verified &&
        items[0].executable_metadata->subject_name) {
      agent_is_verified =
          std::find(subject_names.begin(), subject_names.end(),
                    items[0].executable_metadata->subject_name.value()) !=
          subject_names.end();
    }
  } else {
    DVLOG(1) << __func__ << ": no subject names";
    agent_is_verified = true;
  }

  if (agent_is_verified) {
    DVLOG(1) << __func__ << ": agent is verified";

    is_agent_verified_[config] = true;

    // Re-start all active requests.  If for some reason the request cannot be
    // re-started, break and the LBUS will try again later.
    for (auto& value : active_requests_) {
      if (!ProcessRequest(value.first)) {
        break;
      }
    }
  } else {
    DVLOG(1) << __func__ << ": agent not verified";

    // If the agent is not verified, write something to the syslog so that
    // system administators can see it.
    auto wrapped = ContentAnalysisSdkManager::Get()->GetClient(config);
    std::string path = (wrapped && wrapped->client())
                           ? wrapped->client()->GetAgentInfo().binary_path
                           : std::string("<Agent path unknown>");
    SYSLOG(ERROR) << "Cannot verify content analysis agent: " << path;

    // Force all active requests to fail.
    retry_count_ = kMaxRetryCount;
    RetryActiveRequestsSoonOrFailAllRequests(config);
  }
}

bool LocalBinaryUploadService::IsAgentVerified(
    const content_analysis::sdk::Client::Config& config) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  auto it = is_agent_verified_.find(config);
  return it != is_agent_verified_.end() && it->second;
}

bool LocalBinaryUploadService::IsAgentBeingVerified(
    const content_analysis::sdk::Client::Config& config) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  return is_agent_verified_.find(config) != is_agent_verified_.end();
}

void LocalBinaryUploadService::ResetClient(
    const content_analysis::sdk::Client::Config& config) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << __func__;

  ContentAnalysisSdkManager::Get()->ResetClient(config);
  is_agent_verified_.erase(config);
}

void LocalBinaryUploadService::DoLocalContentAnalysis(Request::Id id,
                                                      Result result,
                                                      Request::Data data) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << __func__ << ": id=" << id;

  if (LocalResultIsFailure(result)) {
    FinishRequest(id, result, ContentAnalysisResponse());
    return;
  }

  auto it = active_requests_.find(id);
  if (it == active_requests_.end())
    return;

  const auto& info = it->second;

  // If this is a retry, the request token is already set.  Don't set
  // it again.
  if (info.request->request_token().empty()) {
    info.request->SetRandomRequestToken();
    DVLOG(1) << __func__ << ": id=" << id
             << " new request_token=" << info.request->request_token();
  } else {
    DVLOG(1) << __func__ << ": id=" << id
             << " existing request_token=" << info.request->request_token();
  }

  DCHECK(info.request->cloud_or_local_settings().is_local_analysis());

  auto config = SDKConfigFromRequest(info.request.get());
  auto wrapped = ContentAnalysisSdkManager::Get()->GetClient(config);
  DCHECK(wrapped && wrapped->client());
  DCHECK(IsAgentVerified(config));

  content_analysis::sdk::ContentAnalysisRequest sdk_request =
      ConvertChromeRequestToSDKRequest(
          info.request->content_analysis_request());

  if (!data.contents.empty()) {
    sdk_request.set_text_content(std::move(data.contents));
  } else if (!data.path.empty()) {
    sdk_request.set_file_path(data.path.AsUTF8Unsafe());
    DVLOG(1) << __func__ << ": id=" << id
             << " file=" << data.path.AsUTF8Unsafe();
  } else if (data.page.IsValid()) {
#if BUILDFLAG(IS_WIN)
    sdk_request.mutable_print_data()->set_handle(
        reinterpret_cast<int64_t>(data.page.GetPlatformHandle()));
    sdk_request.mutable_print_data()->set_size(data.page.GetSize());
#else
    // TODO(b/270942162, b/270941037): Migrate other platforms to handle-based
    // print scanning.
    auto mapping = data.page.Map();
    sdk_request.mutable_text_content()->assign(mapping.GetMemoryAs<char>(),
                                               mapping.size());
#endif
  } else {
    NOTREACHED_IN_MIGRATION();
  }

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::TaskPriority::USER_VISIBLE, base::MayBlock(),
       base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SendRequestToSDK, wrapped, std::move(sdk_request)),
      base::BindOnce(&LocalBinaryUploadService::HandleResponse,
                     factory_.GetWeakPtr(), wrapped, std::move(data)));
}

void LocalBinaryUploadService::HandleResponse(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    safe_browsing::BinaryUploadService::Request::Data data,
    std::optional<content_analysis::sdk::ContentAnalysisResponse>
        sdk_response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << __func__;

  if (!sdk_response.has_value()) {
    // Put the request into the pending queue.  Queue up a call to retry
    // connecting to the agent in order to start processing requests again.
    RetryActiveRequestsSoonOrFailAllRequests(wrapped->client()->GetConfig());
    return;
  }

  retry_count_ = 0;

  // Find the request that corresponds to this response.  It's possible the
  // request is not found if for example it was cancelled by the user or it
  // timed out.
  Request::Id id = FindRequestByToken(sdk_response.value());
  if (id) {
#if defined(_DEBUG)
    DumpSdkAnalysisResponse(__func__, id, sdk_response.value());
#endif

    auto response = ConvertSDKResponseToChromeResponse(sdk_response.value());
    FinishRequest(id, Result::SUCCESS, std::move(response));
    ProcessNextPendingRequest();
  } else {
    DVLOG(1) << __func__
             << ": id not found token=" << sdk_response.value().request_token();
  }
}

void LocalBinaryUploadService::DoSendAck(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    std::unique_ptr<safe_browsing::BinaryUploadService::Ack> ack) {
  if (!wrapped || !wrapped->client()) {
    return;
  }

  content_analysis::sdk::ContentAnalysisAcknowledgement sdk_ack =
      ConvertChromeAckToSDKAck(ack->ack());
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SendAckToSDK, wrapped, std::move(sdk_ack)),
      base::BindOnce(&LocalBinaryUploadService::HandleAckResponse,
                     factory_.GetWeakPtr(), wrapped));
}

void LocalBinaryUploadService::DoSendCancel(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    std::unique_ptr<safe_browsing::BinaryUploadService::CancelRequests>
        cancel) {
  if (!wrapped || !wrapped->client()) {
    return;
  }

  DVLOG(1) << __func__ << ": action_id=" << cancel->get_user_action_id();

  content_analysis::sdk::ContentAnalysisCancelRequests sdk_cancel;
  sdk_cancel.set_user_action_id(cancel->get_user_action_id());

  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      base::BindOnce(&SendCancelToSDK, wrapped, std::move(sdk_cancel)),
      base::BindOnce(&LocalBinaryUploadService::HandleCancelResponse,
                     factory_.GetWeakPtr(), wrapped, std::move(cancel)));
}

void LocalBinaryUploadService::HandleAckResponse(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    int status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (status != 0) {
    ResetClient(wrapped->client()->GetConfig());
  }
}

void LocalBinaryUploadService::HandleCancelResponse(
    scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
    std::unique_ptr<safe_browsing::BinaryUploadService::CancelRequests> cancel,
    int status) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  OnCancelRequestSent(std::move(cancel));

  if (status != 0) {
    ResetClient(wrapped->client()->GetConfig());
  }
}

LocalBinaryUploadService::Request::Id
LocalBinaryUploadService::FindRequestByToken(
    const content_analysis::sdk::ContentAnalysisResponse& sdk_response) {
  // Request must be currently active.
  const auto& request_token = sdk_response.request_token();
  auto it = std::find_if(active_requests_.begin(), active_requests_.end(),
                         [request_token](const auto& value_type) {
                           return request_token ==
                                  value_type.second.request->request_token();
                         });
  return it != active_requests_.end() ? it->first : Request::Id();
}

void LocalBinaryUploadService::ProcessNextPendingRequest() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (pending_requests_.size() > 0) {
    auto info = std::move(pending_requests_.front());
    pending_requests_.erase(pending_requests_.begin());
    Request::Id id = info.request->id();
    active_requests_.emplace(id, std::move(info));
    ProcessRequest(id);
  }
}

bool LocalBinaryUploadService::ProcessRequest(Request::Id id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << __func__ << ": id=" << id;
  DCHECK_GT(active_requests_.count(id), 0u);
  const auto& info = active_requests_.at(id);
  auto config = SDKConfigFromRequest(info.request.get());

  auto wrapped = ContentAnalysisSdkManager::Get()->GetClient(config);
  if (!wrapped || !wrapped->client()) {
    RetryActiveRequestsSoonOrFailAllRequests(config);
    return false;
  }

  if (!IsAgentVerified(config)) {
    StartAgentVerification(
        config, info.request->cloud_or_local_settings().subject_names());
    return true;
  }

  info.request->StartRequest();
  info.request->GetRequestData(
      base::BindOnce(&LocalBinaryUploadService::DoLocalContentAnalysis,
                     factory_.GetWeakPtr(), id));
  return true;
}

void LocalBinaryUploadService::FinishRequest(Request::Id id,
                                             Result result,
                                             ContentAnalysisResponse response) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
#if defined(_DEBUG)
  DumpAnalysisResponse(__func__, id, response);
#endif

  auto it = active_requests_.find(id);
  if (it != active_requests_.end()) {
    const auto& info = it->second;
    RecordRequestMetrics(info, result, response);
    info.request->FinishRequest(result, response);
    active_requests_.erase(id);
  } else {
    DVLOG(1) << __func__ << ": id=" << id << " not active";
  }

  auto it2 = base::ranges::find(
      pending_requests_, id,
      [](const RequestInfo& info) { return info.request->id(); });
  if (it2 != pending_requests_.end()) {
    it2->request->FinishRequest(result, response);
    pending_requests_.erase(it2);
  }

  SendCancelRequestsIfNeeded();
}

void LocalBinaryUploadService::SendCancelRequestsIfNeeded() {
  DVLOG(1) << __func__;

  for (auto it = pending_cancel_requests_.begin();
       it != pending_cancel_requests_.end();) {
    bool cancel_now = true;
    for (auto& pair : active_requests_) {
      if (pair.second.request->user_action_id() ==
          (*it)->get_user_action_id()) {
        cancel_now = false;
        break;
      }
    }

    if (cancel_now) {
      auto curr = it++;
      auto cancel = std::move(pending_cancel_requests_.extract(curr).value());

      // Tell agent to cancel requests.  This is best effort only on the part
      // of the agent.
      auto* cancel_ptr = cancel.get();
      DoSendCancel(ContentAnalysisSdkManager::Get()->GetClient(
                       SDKConfigFromCancel(cancel_ptr)),
                   std::move(cancel));
    } else {
      ++it;
    }
  }
}

void LocalBinaryUploadService::OnTimeout(Request::Id id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << __func__ << ": id=" << id;

  if (active_requests_.count(id) > 0) {
    const auto& info = active_requests_.at(id);
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

  FinishRequest(id, BinaryUploadService::Result::TIMEOUT,
                ContentAnalysisResponse());
  ProcessNextPendingRequest();
}

void LocalBinaryUploadService::RetryActiveRequestsSoonOrFailAllRequests(
    const content_analysis::sdk::Client::Config& config) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << __func__ << ": current-retry-count=" << retry_count_;

  if (ConnectionRetryInProgress()) {
    DVLOG(1) << __func__ << ": retry already in progress, ignore";
    return;
  }

  // An error occurred talking to the agent.  Reset the client so that the
  // next request attempts to reconnect to the agent.
  ResetClient(config);

  // True if requests should be marked as failed.  Otherwise active requests
  // should be moved to the pending list.
  bool fail_requests = retry_count_ >= kMaxRetryCount;

  // Move all active requests to the pending list.
  for (auto it = active_requests_.begin(); it != active_requests_.end();
       it = active_requests_.begin()) {
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
      FinishRequest(it->request->id(),
                    BinaryUploadService::Result::UPLOAD_FAILURE,
                    ContentAnalysisResponse());
    }
  } else {
    StartConnectionRetry();
  }
}

void LocalBinaryUploadService::StartConnectionRetry() {
  DVLOG(1) << __func__;
  if (!ConnectionRetryInProgress()) {
    ++retry_count_;
    connection_retry_timer_.Start(
        FROM_HERE, retry_count_ * base::Milliseconds(100),
        base::BindOnce(&LocalBinaryUploadService::OnConnectionRetry,
                       factory_.GetWeakPtr()));
  }
}

void LocalBinaryUploadService::OnConnectionRetry() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DVLOG(1) << __func__;

  // ProcessNextPendingRequest() moves one request from the pending list to
  // the active list and then calls ProcessRequest().  The latter could move
  // the request back to the pending list should an error occur.  To avoid an
  // endless loop, calculate the number of requests that should be moved and
  // only move that many.
  size_t num_requests_to_process = std::min(
      kMaxActiveCount - active_requests_.size(), pending_requests_.size());
  for (size_t i = 0; i < num_requests_to_process; ++i) {
    ProcessNextPendingRequest();
  }
}

bool LocalBinaryUploadService::ConnectionRetryInProgress() {
  return connection_retry_timer_.IsRunning();
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
