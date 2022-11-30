// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_NETWORK_QUALITY_ESTIMATOR_PROVIDER_IMPL_H_
#define CHROME_BROWSER_METRICS_NETWORK_QUALITY_ESTIMATOR_PROVIDER_IMPL_H_

#include "base/threading/thread_checker.h"
#include "components/metrics/net/network_metrics_provider.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace metrics {

// Implements NetworkMetricsProvider::NetworkQualityEstimatorProvider. Provides
// network quality estimates. Lives on UI thread.
class NetworkQualityEstimatorProviderImpl
    : public NetworkMetricsProvider::NetworkQualityEstimatorProvider,
      public network::NetworkQualityTracker::EffectiveConnectionTypeObserver {
 public:
  NetworkQualityEstimatorProviderImpl();

  NetworkQualityEstimatorProviderImpl(
      const NetworkQualityEstimatorProviderImpl&) = delete;
  NetworkQualityEstimatorProviderImpl& operator=(
      const NetworkQualityEstimatorProviderImpl&) = delete;

  ~NetworkQualityEstimatorProviderImpl() override;

 private:
  // NetworkMetricsProvider::NetworkQualityEstimatorProvider:
  void PostReplyOnNetworkQualityChanged(
      base::RepeatingCallback<void(net::EffectiveConnectionType)> callback)
      override;

  // network::NetworkQualityTracker::EffectiveConnectionTypeObserver:
  void OnEffectiveConnectionTypeChanged(
      net::EffectiveConnectionType type) override;

  void AddEffectiveConnectionTypeObserverNow(
      base::RepeatingCallback<void(net::EffectiveConnectionType)> callback);

  // |callback_| is invoked every time there is a change in the network quality
  // estimate. May be null.
  base::RepeatingCallback<void(net::EffectiveConnectionType)> callback_;

  THREAD_CHECKER(thread_checker_);

  base::WeakPtrFactory<NetworkQualityEstimatorProviderImpl> weak_ptr_factory_{
      this};
};

}  // namespace metrics

#endif  // CHROME_BROWSER_METRICS_NETWORK_QUALITY_ESTIMATOR_PROVIDER_IMPL_H_
