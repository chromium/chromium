// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_NETWORK_CONDITION_SERVICE_H_
#define CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_NETWORK_CONDITION_SERVICE_H_

#include <atomic>
#include <cstddef>
#include <limits>

#include "base/gtest_prod_util.h"
#include "base/memory/singleton.h"
#include "chrome/browser/browser_process.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace reporting {

// Provide current network condition.
// This is a singleton instance that subscribes to
// g_browser_process->network_quality_tracker(). An alternative is to create an
// instance of NetworkConditionService each time an upload starts. However,
// because NetworkConditionService estimates upload rate by posting to the UI
// thread a task to subscribe, it may well be possible that the upload rate has
// not been estimated even after the upload has finished.
class NetworkConditionService
    : public network::NetworkQualityTracker::RTTAndThroughputEstimatesObserver {
 public:
  NetworkConditionService(const NetworkConditionService&) = delete;
  NetworkConditionService& operator=(const NetworkConditionService&) = delete;

  // Get the Singleton |NetworkConditionService| instance.
  static NetworkConditionService* GetInstance();

  // Get current upload rate.
  uint64_t GetUploadRate() const;

 private:
  friend struct base::DefaultSingletonTraits<NetworkConditionService>;
  friend class NetworkConditionServiceTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkConditionServiceTest,
                           SuccessfulInitializationAndUpdate);

  // Convert kbps (kilobits per second) to bytes per second.
  static uint64_t ConvertKbpsToBytesPerSec(int32_t kbps);

  NetworkConditionService();

  // Set upload rate from kilobits per second.
  NetworkConditionService& SetUploadRateKbps(int32_t upload_rate_kbps);
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

  // The current upload rate. Set it to the maximum possible number by
  // default before any estimate is given.
  std::atomic<uint64_t> upload_rate_{std::numeric_limits<uint64_t>::max()};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_POLICY_MESSAGING_LAYER_UPLOAD_NETWORK_CONDITION_SERVICE_H_
