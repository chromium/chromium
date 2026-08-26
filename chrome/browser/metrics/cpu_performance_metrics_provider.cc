// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/cpu_performance_metrics_provider.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace metrics {

// static
CpuPerformanceTierOverridePair CpuPerformanceMetricsProvider::EncodePair(
    content::cpu_performance::Tier nominal_tier,
    content::cpu_performance::Tier override_tier) {
  CHECK_LE(content::cpu_performance::Tier::kMinValue, nominal_tier);
  CHECK_GE(content::cpu_performance::Tier::kMaxValue, nominal_tier);
  int nominal_val = static_cast<int>(nominal_tier);

  CHECK_LE(content::cpu_performance::Tier::kMinValue, override_tier);
  CHECK_GE(content::cpu_performance::Tier::kMaxValue, override_tier);
  int override_val = static_cast<int>(override_tier);

  // This encoding works for tier values between 0 (UNKNOWN) and 4 (ULTRA).
  // If new tiers are added in the future, the encoding must ensure that
  // the numbering of the existing pairs will not be affected and that the
  // new pairs will be properly encoded as higher values.
  static_assert(static_cast<int>(content::cpu_performance::Tier::kMinValue) ==
                0);
  static_assert(static_cast<int>(content::cpu_performance::Tier::kMaxValue) ==
                4);
  int encoded_val = 5 * nominal_val + override_val;
  return static_cast<CpuPerformanceTierOverridePair>(encoded_val);
}

void CpuPerformanceMetricsProvider::ProvideCurrentSessionData(
    ChromeUserMetricsExtension* uma_proto) {
  Profile* profile = cached_profile_.GetMetricsProfile();
  if (!profile) {
    return;
  }

  const PrefService* prefs = profile->GetPrefs();
  if (!prefs) {
    return;
  }

  const int override_val =
      prefs->GetInteger(prefs::kCpuPerformanceTierOverride);
  if (override_val <
          static_cast<int>(content::cpu_performance::Tier::kMinValue) ||
      override_val >
          static_cast<int>(content::cpu_performance::Tier::kMaxValue)) {
    // If the setting is not overridden
    // (kCpuPerformanceTierOverrideNone == -1 < kMinValue)
    // or if the user preference value is corrupted (out of range),
    // do not log anything.
    return;
  }
  const auto override_tier =
      static_cast<content::cpu_performance::Tier>(override_val);
  const auto nominal_tier = content::cpu_performance::GetTier();
  const auto sample = EncodePair(nominal_tier, override_tier);

  if (prefs->IsManagedPreference(prefs::kCpuPerformanceTierOverride)) {
    base::UmaHistogramEnumeration(
        "PerformanceControls.CpuPerformanceTier.PolicyOverride", sample);
  } else {
    base::UmaHistogramEnumeration(
        "PerformanceControls.CpuPerformanceTier.UserOverride", sample);
  }
}

}  // namespace metrics
