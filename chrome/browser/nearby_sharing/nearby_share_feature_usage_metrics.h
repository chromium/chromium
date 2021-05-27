// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_USAGE_METRICS_H_
#define CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_USAGE_METRICS_H_

#include "chromeos/components/feature_usage/feature_usage_metrics.h"

class PrefService;

// Tracks Nearby Share feature usage for the Standard Feature Usage Logging
// (SFUL) framework.
class NearbyShareFeatureUsageMetrics
    : public feature_usage::FeatureUsageMetrics::Delegate {
 public:
  // These values are persisted to logs. Entries should not be renumbered and
  // numeric values should never be reused. If entries are added, kMaxValue
  // should be updated.
  enum class NearbyShareEnabledState {
    kEnabledAndOnboarded = 0,
    kEnabledAndNotOnboarded = 1,
    kDisabledAndOnboarded = 2,
    kDisabledAndNotOnboarded = 3,
    kDisallowedByPolicy = 4,
    kMaxValue = kDisallowedByPolicy
  };

  explicit NearbyShareFeatureUsageMetrics(PrefService* pref_service);
  ~NearbyShareFeatureUsageMetrics() final;

  // feature_usage::FeatureUsageMetrics::Delegate:
  bool IsEligible() const final;
  bool IsEnabled() const final;

  NearbyShareEnabledState GetNearbyShareEnabledState() const;
  void RecordUsage(bool success);

 private:
  PrefService* pref_service_;
  feature_usage::FeatureUsageMetrics feature_usage_metrics_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_NEARBY_SHARE_FEATURE_USAGE_METRICS_H_
