// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_NETWORK_CONDITION_SERVICE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_NETWORK_CONDITION_SERVICE_H_

#include <atomic>
#include <cstddef>
#include <limits>
#include <memory>

#include "base/gtest_prod_util.h"
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
  class NetworkConditionServiceObserver
      : public network::NetworkQualityTracker::
            RTTAndThroughputEstimatesObserver {
   public:
    // a unique_ptr object that has a deleter.
    using UniquePtr =
        std::unique_ptr<NetworkConditionServiceObserver,
                        void (*)(NetworkConditionServiceObserver*)>;

    explicit NetworkConditionServiceObserver(
        NetworkConditionService* network_condition_service);

    // Create a unique pointer that uses CallDestroy as the
    // deleter. network_condition_service is pointer to the owning
    // |NetworkConditionService| instance.
    static UniquePtr MakeUnique(
        NetworkConditionService* network_condition_service);

   private:
    // Must be destroyed via |Destroy|.
    ~NetworkConditionServiceObserver() override = default;

    // A simple wrapper that calls |Destroy|. Used as a deleter in a unique_ptr
    // object.
    static void CallDestroy(NetworkConditionServiceObserver* observer);

    // Unregister the observer on the UI thread first and destroys this
    // object. Should be only called once when |NetworkConditionService|
    // destructs.
    void Destroy();
    // Register this as an |RTTAndThroughputEstimatesObserver|. Only to be
    // called from the UI thread.
    void RegisterRTTAndThroughputEstimatesObserver();
    // Overriding OnRTTOrThroughputEstimatesComputed in
    // |network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver| to
    // update upload_rate_ whenever g_browser_process->network_quality_tracker()
    // notifies downstream throughput change.
    void OnRTTOrThroughputEstimatesComputed(
        base::TimeDelta http_rtt,
        base::TimeDelta transport_rtt,
        int32_t downstream_throughput_kbps) override;

    // The |NetworkConditionService| object that own this object.
    NetworkConditionService* network_condition_service_;
  };

  friend class NetworkConditionServiceObserver;
  friend class NetworkConditionServiceTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkConditionServiceTest,
                           SuccessfulInitializationAndUpdateAndDestroy);

  // Convert kbps (kilobits per second) to bytes per second.
  static uint64_t ConvertKbpsToBytesPerSec(int32_t kbps);

  // Set upload rate from kilobits per second.
  NetworkConditionService& SetUploadRateKbps(int32_t upload_rate_kbps);

  // The current upload rate. Set it to the maximum possible number by
  // default before any estimate is given.
  std::atomic<uint64_t> upload_rate_{std::numeric_limits<uint64_t>::max()};
  // The observer that subscribes to changes in network condition.
  NetworkConditionServiceObserver::UniquePtr observer_{
      NetworkConditionServiceObserver::MakeUnique(this)};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_NETWORK_CONDITION_SERVICE_H_
