// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/reporting/network/network_bandwidth_sampler.h"

#include <limits>

#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "services/network/public/cpp/network_quality_tracker.h"

namespace reporting {

// static
BASE_FEATURE(kEnableNetworkBandwidthReporting,
             "EnableNetworkBandwidthReporting",
             base::FEATURE_ENABLED_BY_DEFAULT);

NetworkBandwidthSampler::NetworkBandwidthSampler(
    ::network::NetworkQualityTracker* network_quality_tracker,
    base::WeakPtr<Profile> profile)
    : network_quality_tracker_(network_quality_tracker), profile_(profile) {
  CHECK(network_quality_tracker_);
  CHECK(profile_);
}

NetworkBandwidthSampler::~NetworkBandwidthSampler() = default;

void NetworkBandwidthSampler::MaybeCollect(OptionalMetricCallback callback) {
  if (!::content::BrowserThread::CurrentlyOn(::content::BrowserThread::UI)) {
    ::content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&NetworkBandwidthSampler::MaybeCollect,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
    return;
  }
  if (!profile_) {
    // Profile destructed so we collect no data.
    std::move(callback).Run(std::nullopt);
    return;
  }
  if (!profile_->GetPrefs()->GetBoolean(::prefs::kInsightsExtensionEnabled) &&
      !base::FeatureList::IsEnabled(kEnableNetworkBandwidthReporting)) {
    // Both policy and feature flag not set, so we return.
    std::move(callback).Run(std::nullopt);
    return;
  }

  const auto downlink_speed_kbps =
      network_quality_tracker_->GetDownstreamThroughputKbps();
  if (downlink_speed_kbps == std::numeric_limits<int32_t>::max()) {
    // Network quality tracker returns std::numeric_limits<int32_t>::max() if
    // downstream throughput is unavailable, so we return no data.
    std::move(callback).Run(std::nullopt);
    return;
  }

  MetricData metric_data;
  metric_data.mutable_telemetry_data()
      ->mutable_networks_telemetry()
      ->mutable_bandwidth_data()
      ->set_download_speed_kbps(downlink_speed_kbps);
  std::move(callback).Run(metric_data);
}

}  // namespace reporting
