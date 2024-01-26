// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_

#include <map>
#include <memory>
#include <set>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "components/device_signals/core/common/mojom/system_signals.mojom.h"

namespace enterprise_connectors {

// This class encapsulates the process of sending a file to local content
// analysis agents for deep scanning and asynchronously retrieving a verdict.
// This class runs on the UI thread.
class LocalBinaryUploadService : public safe_browsing::BinaryUploadService {
 public:
  // the maximum number of concurrently active requests to the local content
  // analysis agent.
  static constexpr size_t kMaxActiveCount = 5;

  // The maximum number of reconnection retries chrome will attempt when an
  // error occurs with the agent communication.  Once this is reached chrome
  // will no longer attempt to connect to the agent until it restarts.
  static constexpr size_t kMaxRetryCount = 1;

  // The maximum amount of time chrome will wait for a verdict from the local
  // content analysis agent.
  static constexpr base::TimeDelta kScanningTimeout = base::Minutes(5);

  // Keeps track and owns requests sent to the local agent for deep scanning.
  // When RequestInfo is created a timer is started to handle agent timeouts.
  // If the timer expires before a response is returned for this requesdt, then
  // LocalBinaryUploadService will respond to the request with an
  // UPLOAD_FAILURE and will send an ack back to the agent that it took too
  // long.
  //
  // The move ctor and dtor are declared noexcept so that when placed inside
  // a vector<>, the move ctor is used instead of the copy ctor (which is
  // deleted).
  struct RequestInfo {
    RequestInfo(std::unique_ptr<Request> request, base::OnceClosure closure);
    RequestInfo(const RequestInfo& other) = delete;
    RequestInfo(RequestInfo&& other) noexcept;
    RequestInfo& operator=(const RequestInfo& other) = delete;
    RequestInfo& operator=(RequestInfo&& other) noexcept;
    ~RequestInfo() noexcept;

    base::TimeTicks started_at;
    std::unique_ptr<Request> request;
    std::unique_ptr<base::OneShotTimer> timer;
  };

  explicit LocalBinaryUploadService(Profile* profile);
  ~LocalBinaryUploadService() override;

  // Send the given file contents to local partners for deep scanning.
  void MaybeUploadForDeepScanning(std::unique_ptr<Request> request) override;
  void MaybeAcknowledge(std::unique_ptr<Ack> ack) override;
  void MaybeCancelRequests(std::unique_ptr<CancelRequests> cancel) override;
  base::WeakPtr<BinaryUploadService> AsWeakPtr() override;

  size_t GetActiveRequestCountForTesting() const {
    return active_requests_.size();
  }

  size_t GetPendingRequestCountForTesting() const {
    return pending_requests_.size();
  }

  const std::map<Request::Id, RequestInfo>& GetActiveRequestsForTesting()
      const {
    return active_requests_;
  }

  const std::vector<RequestInfo>& GetPendingRequestsForTesting() const {
    return pending_requests_;
  }

  void OnTimeoutForTesting(Request::Id id) { OnTimeout(id); }

 protected:
  // Map to keep track of whether the agent's authenticity has been
  // verified and if a check is in progress.  If a given agent's `config` is
  // not in the map, then the agent is not verified and no verification is
  // in progress.  If the `config` is in the map and the value is false,
  // an agent verification is in progress.  If the `config` is in the map
  // and the value is true, the agent has been verified.
  using AgentVerifiedMap =
      std::map<content_analysis::sdk::Client::Config,
               bool,
               decltype(ContentAnalysisSdkManager::CompareConfig)>;

  AgentVerifiedMap& GetAgentVerifiedMapForTesting() {
    return is_agent_verified_;
  }

  // Gets the SystemSignalsService to extract the subject name from the
  // agent.  This method is virtual to allow overriding in tests.
  virtual device_signals::mojom::SystemSignalsService*
  GetSystemSignalsService();

  // Starts verification of the agent specified by the given config.  This
  // method virtual so that tests can override the dependency on
  // SystemsignalsServiceHost.
  virtual void StartAgentVerification(
      const content_analysis::sdk::Client::Config& config,
      base::span<const char* const> subject_names);

  // Called when the verification of the agent specified by the given config
  // is complete.
  void OnFileSystemSignals(
      content_analysis::sdk::Client::Config config,
      base::span<const char* const> subject_names,
      const std::vector<device_signals::FileSystemItem>& items);

  // Determines if the authenticity of the agent specified by the given config
  // has been or is being verified.  This method virtual so that tests can
  // override the dependency on SystemsignalsServiceHost.
  virtual bool IsAgentVerified(
      const content_analysis::sdk::Client::Config& config);
  virtual bool IsAgentBeingVerified(
      const content_analysis::sdk::Client::Config& config);

 private:
  // If an error occurs with a client, reset its state.
  void ResetClient(const content_analysis::sdk::Client::Config& config);

  // Starts a local content analysis for the analysis request given by `id`.
  void DoLocalContentAnalysis(Request::Id id,
                              Result result,
                              Request::Data data);

  // Handles a response from the agent for a given request.
  // `data` is not used directly by this function, but is needed to keep a
  // scoped handle alive.
  void HandleResponse(
      scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
      safe_browsing::BinaryUploadService::Request::Data data,
      std::optional<content_analysis::sdk::ContentAnalysisResponse>
          sdk_response);

  // Starts a local content analysis ack request.
  void DoSendAck(
      scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
      std::unique_ptr<safe_browsing::BinaryUploadService::Ack> ack);

  // Starts a local content analysis cancel request.
  void DoSendCancel(
      scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
      std::unique_ptr<safe_browsing::BinaryUploadService::CancelRequests>
          cancel);

  // Handles a response from the agent for a given ask or cancel.
  void HandleAckResponse(
      scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
      int status);
  void HandleCancelResponse(
      scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
      std::unique_ptr<safe_browsing::BinaryUploadService::CancelRequests>
          cancel,
      int status);

  // In tests, this method can be overridden to know when a cancel request
  // has been sent to the agent.
  virtual void OnCancelRequestSent(std::unique_ptr<CancelRequests> cancel) {}

  // Find the request that corresponds to the given response.
  Request::Id FindRequestByToken(
      const content_analysis::sdk::ContentAnalysisResponse& sdk_response);

  // Move the next request from the pending list, if any, to the active
  // list and process it.
  void ProcessNextPendingRequest();

  // Starts the request given by `id` that is already on the active
  // list.  If this function returns true the active request list is still
  // valid.  Otherwise the active request list has been cleared.
  bool ProcessRequest(Request::Id id);

  // Finish the request given by `id` and inform caller of the the resulting
  // verdict.
  void FinishRequest(Request::Id id,
                     Result result,
                     ContentAnalysisResponse response);

  // Send a cancel request to the agent if there are no more active requests
  // for the given action.
  void SendCancelRequestsIfNeeded();

  // Handles a timeout for the request given by `id`.  The request could
  // be in either the active or pending lists.
  void OnTimeout(Request::Id id);

  // If there haven't been too many retries, moves all requests from the active
  // list to the pending list and queues up a task to reconnect to the agent.
  // Once reconnected the requests will be retried in order.
  //
  // If there have been too many errors connecting to the agent, fail all
  // active and pending requests.  No more attempts will be made to reconnect
  // to the agent and that all subsequent deep scan requests should fail
  // automatically.
  void RetryActiveRequestsSoonOrFailAllRequests(
      const content_analysis::sdk::Client::Config& config);

  // Attempts to reconnect to agent if a connection is not already in progress.
  void StartConnectionRetry();

  // Called when BinaryUploadService should attempt to reconnect and retry
  // requests to the agent.  This method is called by the timer set in
  // RetryRequest().
  void OnConnectionRetry();

  // This method returns true when a connection retry timer is in progress.
  bool ConnectionRetryInProgress();

  void RecordRequestMetrics(
      const RequestInfo& info,
      Result result,
      const enterprise_connectors::ContentAnalysisResponse& response);

  raw_ptr<Profile> profile_;

  Request::Id::Generator request_id_generator_;

  // Keeps track of outstanding requests sent to the agent.
  std::map<Request::Id, RequestInfo> active_requests_;

  // Keeps track of pending requests not yet sent.
  std::vector<RequestInfo> pending_requests_;

  // Pending cancel requests.  Once all requests associated with a given
  // action are sent, a cancel request can be sent.  This is to ensure that
  // chrome does not send requests for analysis for a given action after a
  // cancel request has been sent.
  std::set<std::unique_ptr<CancelRequests>> pending_cancel_requests_;

  // Timer used to retry connection to agent.
  base::OneShotTimer connection_retry_timer_;

  // Number of times LBUS has retried to connect to agent.  This count is
  // reset once a successful connection is established.
  size_t retry_count_ = 0;

  // Map to keep track of whether the agent's authenticity has been
  // verified and if a check is in progress.
  AgentVerifiedMap is_agent_verified_;

  // As the last data member of class.
  base::WeakPtrFactory<LocalBinaryUploadService> factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_
