// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_HUMAN_PRESENCE_HUMAN_PRESENCE_METRICS_H_
#define ASH_SYSTEM_HUMAN_PRESENCE_HUMAN_PRESENCE_METRICS_H_

#include "base/time/time.h"

namespace ash {

// Use two namespaces to keep constant names legible.
namespace snooping_protection_metrics {

constexpr char kEnabledHistogramName[] =
    "ChromeOS.HPS.SnoopingProtection.Enabled";
constexpr char kPositiveDurationHistogramName[] =
    "ChromeOS.HPS.SnoopingProtection.Positive.Duration";
constexpr char kNegativeDurationHistogramName[] =
    "ChromeOS.HPS.SnoopingProtection.Negative.Duration";
constexpr char kFlakeyHistogramName[] =
    "ChromeOS.HPS.SnoopingProtection.FlakeyDetection";
constexpr char kNotificationSuppressionEnabledHistogramName[] =
    "ChromeOS.HPS.SnoopingProtectionNotificationSuppression.Enabled";

// Number of buckets to log SnoopingProtection present result.
constexpr int kDurationNumBuckets = 100;

// Minimum value for the SnoopingProtection.Positive.Duration and
// SnoopingProtection.Negative.Duration.
constexpr base::TimeDelta kDurationMin = base::Seconds(1);

// Maximum value for SnoopingProtection.Positive.Duration; Longer than 1 hour is
// considered as 1 hour.
constexpr base::TimeDelta kPositiveDurationMax = base::Hours(1);

// Maximum value for SnoopingProtection.Negative.Duration; Longer than 1 day is
// considered as 1 day.
constexpr base::TimeDelta kNegativeDurationMax = base::Hours(24);

}  // namespace snooping_protection_metrics

namespace quick_dim_metrics {

constexpr char kEnabledHistogramName[] = "ChromeOS.HPS.QuickDim.Enabled";

}  // namespace quick_dim_metrics

}  // namespace ash

#endif  // ASH_SYSTEM_HUMAN_PRESENCE_HUMAN_PRESENCE_METRICS_H_
