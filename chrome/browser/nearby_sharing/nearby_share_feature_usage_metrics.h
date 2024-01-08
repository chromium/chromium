// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_USAGE_METRICS_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_USAGE_METRICS_H_

#include "base/memory/raw_ptr.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"

class PrefService;

// Tracks Nearby Share feature usage for the Standard Feature Usage Logging
// (SFUL) framework.
class NearbyShareFeatureUsageMetrics final
    : public ash::feature_usage::FeatureUsageMetrics::Delegate {
 public:
  explicit NearbyShareFeatureUsageMetrics(PrefService* pref_service);
  NearbyShareFeatureUsageMetrics(NearbyShareFeatureUsageMetrics&) = delete;
  NearbyShareFeatureUsageMetrics& operator=(NearbyShareFeatureUsageMetrics&) =
      delete;
  ~NearbyShareFeatureUsageMetrics() override;

  // ash::feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const override;
  bool IsEnabled() const override;
  void RecordUsage(bool success);

 private:
  raw_ptr<PrefService> pref_service_;
  ash::feature_usage::FeatureUsageMetrics feature_usage_metrics_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_USAGE_METRICS_H_
