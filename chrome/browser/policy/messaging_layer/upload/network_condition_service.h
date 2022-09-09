// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_NETWORK_CONDITION_SERVICE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_NETWORK_CONDITION_SERVICE_H_

#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>

#include "base/gtest_prod_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/browser_process.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace reporting {

// Provide current network condition.
class NetworkConditionService {
 public:
  NetworkConditionService();
  NetworkConditionService(const NetworkConditionService&) = delete;
  NetworkConditionService& operator=(const NetworkConditionService&) = delete;
  virtual ~NetworkConditionService();

  // Get current upload rate.
  uint64_t GetUploadRate() const;

 private:
  class NetworkConditionServiceImpl : public network::NetworkQualityTracker::
                                          RTTAndThroughputEstimatesObserver {
   public:
    // a unique_ptr object that has a deleter.
    using UniquePtr =
        std::unique_ptr<NetworkConditionServiceImpl, base::OnTaskRunnerDeleter>;

    // Create a unique pointer that uses CallDestroy as the
    // deleter. network_condition_service is pointer to the owning
    // |NetworkConditionService| instance.
    static UniquePtr MakeUnique();

    // Must be destroyed via |base::OnTaskRunnerDeleter| on the UI thread.
    ~NetworkConditionServiceImpl() override;

    // Get current upload rate.
    uint64_t GetUploadRate() const;

   private:
    friend class NetworkConditionServiceTest;
    FRIEND_TEST_ALL_PREFIXES(NetworkConditionServiceTest,
                             SuccessfulInitializationAndUpdateAndDestroy);
    friend class TestingNetworkConditionService;

    NetworkConditionServiceImpl();

    // Convert kbps (kilobits per second) to bytes per second.
    static uint64_t ConvertKbpsToBytesPerSec(int32_t kbps);

    // Set upload rate from kilobits per second.
    NetworkConditionServiceImpl& SetUploadRateKbps(int32_t upload_rate_kbps);
    // Register this as an |RTTAndThroughputEstimatesObserver|. Only to be
    // called from the UI thread.
    void RegisterRTTAndThroughputEstimatesObserver();
    // Overriding OnRTTOrThroughputEstimatesComputed in
    // |network::NetworkQualityTracker::RTTAndThroughputEstimatesImpl| to
    // update upload_rate_ whenever g_browser_process->network_quality_tracker()
    // notifies downstream throughput change.
    void OnRTTOrThroughputEstimatesComputed(
        base::TimeDelta http_rtt,
        base::TimeDelta transport_rtt,
        int32_t downstream_throughput_kbps) override;

    // The current upload rate. Set it to the maximum possible number by
    // default before any estimate is given.
    std::atomic<uint64_t> upload_rate_{std::numeric_limits<uint64_t>::max()};
  };

  friend class NetworkConditionServiceTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkConditionServiceTest,
                           SuccessfulInitializationAndUpdateAndDestroy);
  friend class TestingNetworkConditionService;

  // The implementation instance.
  NetworkConditionServiceImpl::UniquePtr impl_{
      NetworkConditionServiceImpl::MakeUnique()};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_NETWORK_CONDITION_SERVICE_H_
