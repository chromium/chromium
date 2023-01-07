// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_

#include <map>
#include <memory>
#include <vector>

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/enterprise/connectors/analysis/content_analysis_sdk_manager.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"

namespace enterprise_connectors {

// This class encapsulates the process of sending a file to local content
// analysis agents for deep scanning and asynchronously retrieving a verdict.
// This class runs on the UI thread.
class LocalBinaryUploadService : public safe_browsing::BinaryUploadService {
 public:
  // A value that is used as a unique id for a given Request.  Internally this
  // is just the address of the Request object but the code does not assume
  // this is the case.
  using RequestKey = void*;

  // the maximum number of concurrently active requests to the local content
  // analysis agent.
  static constexpr size_t kMaxActiveCount = 5;

  // The maximum number of reconnection retries chrome will attempt when an
  // error occurs with the agent communication.  Once this is reached chrome
  // will no longer attempt to connect to the agent until it restarts.
  static constexpr size_t kMaxRetryCount = 5;

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

  LocalBinaryUploadService();
  ~LocalBinaryUploadService() override;

  // Send the given file contents to local partners for deep scanning.
  void MaybeUploadForDeepScanning(std::unique_ptr<Request> request) override;
  void MaybeAcknowledge(std::unique_ptr<Ack> ack) override;
  void MaybeCancelRequests(std::unique_ptr<CancelRequests> cancel) override;

  size_t GetActiveRequestCountForTesting() const {
    return active_requests_.size();
  }

  size_t GetPendingRequestCountForTesting() const {
    return pending_requests_.size();
  }

  const std::map<RequestKey, RequestInfo>& GetActiveRequestsForTesting() const {
    return active_requests_;
  }

  const std::vector<RequestInfo>& GetPendingRequestsForTesting() const {
    return pending_requests_;
  }

  void OnTimeoutForTesting(RequestKey key) { OnTimeout(key); }

 private:
  // Starts a local content analysis for the request given by `key`.
  void DoLocalContentAnalysis(RequestKey key,
                              Result result,
                              Request::Data data);

  // Handles a response from the agent for a given request.
  void HandleResponse(
      scoped_refptr<ContentAnalysisSdkManager::WrappedClient> wrapped,
      absl::optional<content_analysis::sdk::ContentAnalysisResponse>
          sdk_response);

  // Find the request that corresponds to the given response.
  RequestKey FindRequestByToken(
      const content_analysis::sdk::ContentAnalysisResponse& sdk_response);

  // Move the next request from the pending list, if any, to the active
  // list and process it.
  void ProcessNextPendingRequest();

  // Starts the request given by `key` that is already on the active
  // list.
  void ProcessRequest(RequestKey key);

  // Finish the request given by `key` and inform caller of the the resulting
  // verdict.
  void FinishRequest(RequestKey key,
                     Result result,
                     ContentAnalysisResponse response);

  // Handles a timeout for the request given by `key`.  The request could
  // be in either the active or pending lists.
  void OnTimeout(RequestKey key);

  // If there haven't been too many retries, moves all requests from the active
  // list to the pending list and queues up a task to reconnect to the agent.
  // Once reconnected the requests will be retried in order.
  //
  // If there have been too many errors connecting to the agent, fail all
  // active and pending requests.  No more attempts will be made to reconnect
  // to the agent and that all subsequent deep scan requests should fail
  // automatically.
  void RetryActiveRequestsSoonOrFailAllRequests();

  // Called when BinaryUploadService should attempt to reconnect and retry
  // requests to the agent.  This method is called by the timer set in
  // RetryRequest().
  void OnConnectionRetry();

  void RecordRequestMetrics(
      const RequestInfo& info,
      Result result,
      const enterprise_connectors::ContentAnalysisResponse& response);

  // Keeps track of outstanding requests sent to the agent.
  std::map<RequestKey, RequestInfo> active_requests_;

  // Keeps track of pending requests not yet sent.
  std::vector<RequestInfo> pending_requests_;

  // Timer used to retry connection to agent.
  base::OneShotTimer connection_retry_timer_;

  // Number of times LBUS has retried to connect to agent.  This count is
  // reset once a successful connection is established.
  size_t retry_count_ = 0;

  // As the last data member of class.
  base::WeakPtrFactory<LocalBinaryUploadService> factory_{this};
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_ANALYSIS_LOCAL_BINARY_UPLOAD_SERVICE_H_
