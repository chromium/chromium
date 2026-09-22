// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_GLIC_METRICS_PROVIDER_H_
#define CHROME_BROWSER_GLIC_GLIC_METRICS_PROVIDER_H_

#include "components/metrics/metrics_provider.h"

class Profile;

namespace glic {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// LINT.IfChange(GlicProfilesAllSomeNone)
enum class GlicProfilesAllSomeNone {
  kAll = 0,
  kSome = 1,
  kNone = 2,

  kMaxValue = kNone,
};
// LINT.ThenChange(//tools/metrics/histograms/metadata/glic/enums.xml:GlicProfilesAllSomeNone)

class GlicMetricsProvider : public metrics::MetricsProvider {
 public:
  GlicMetricsProvider();
  ~GlicMetricsProvider() override;

  // Reconciles the promotion source cohort across all loaded profiles (plus
  // `initializing_profile` if provided before it is added to `ProfileManager`)
  // and registers the `GlicPromotionSourceSynthetic` field trial. If all
  // profiles with a recorded cohort agree, that cohort is registered; if two
  // or more profiles have different recorded cohorts, "MultiProfileDetected"
  // is registered.
  static void RegisterPromotionSourceSyntheticTrial(
      Profile* initializing_profile = nullptr);

  // metrics::MetricsProvider:
  void ProvideCurrentSessionData(
      metrics::ChromeUserMetricsExtension* uma_proto) override;
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_GLIC_METRICS_PROVIDER_H_
