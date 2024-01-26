// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_NETWORK_NETWORK_BANDWIDTH_SAMPLER_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_NETWORK_NETWORK_BANDWIDTH_SAMPLER_H_

#include "base/feature_list.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/metrics/sampler.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace reporting {

// Feature flag that controls network bandwidth reporting for affiliated
// ChromeOS users/devices and is used for phased rollout.
BASE_DECLARE_FEATURE(kEnableNetworkBandwidthReporting);

// Sampler used to collect network bandwidth information like download speed
// (in kbps). This sampler relies on the `NetworkQualityTracker` defined in
// the `NetworkService` to collect said metrics.
class NetworkBandwidthSampler : public Sampler {
 public:
  NetworkBandwidthSampler(
      ::network::NetworkQualityTracker* network_quality_tracker,
      base::WeakPtr<Profile> profile);
  NetworkBandwidthSampler(const NetworkBandwidthSampler& other) = delete;
  NetworkBandwidthSampler& operator=(const NetworkBandwidthSampler& other) =
      delete;
  ~NetworkBandwidthSampler() override;

  // Collects network bandwidth info if the corresponding user policy is set
  // and reports collected metrics using the specified callback. Reports
  // `std::nullopt` otherwise.
  void MaybeCollect(OptionalMetricCallback callback) override;

 private:
  const raw_ptr<::network::NetworkQualityTracker> network_quality_tracker_;
  const base::WeakPtr<const Profile> profile_;

  base::WeakPtrFactory<NetworkBandwidthSampler> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_NETWORK_NETWORK_BANDWIDTH_SAMPLER_H_
