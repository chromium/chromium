// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_TELEMETRY_SAMPLER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_TELEMETRY_SAMPLER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

// Sampler used to collect app usage telemetry from the user profile
// pref store. This usage data is originally persisted in the pref store by the
// `AppUsageObserver` as it observes and tracks data collection from the
// `AppPlatformMetrics` component.
class AppUsageTelemetrySampler : public Sampler {
 public:
  explicit AppUsageTelemetrySampler(base::WeakPtr<Profile> profile);
  AppUsageTelemetrySampler(const AppUsageTelemetrySampler& other) = delete;
  AppUsageTelemetrySampler& operator=(const AppUsageTelemetrySampler& other) =
      delete;
  ~AppUsageTelemetrySampler() override;

  // Collects apps usage telemetry data from the user pref store across several
  // instances and triggers the specified callback with batched usage data.
  // Sampler:
  void MaybeCollect(OptionalMetricCallback callback) override;

 private:
  // Resets app usage entries in the pref store by discounting reported usage
  // time. Triggered only after the data has been consumed to avoid data loss.
  void ResetAppUsageDataInPrefStore(const AppUsageData* app_usage_data);

  // Weak pointer to the user profile. Used to access the user pref store.
  const base::WeakPtr<Profile> profile_;

  base::WeakPtrFactory<AppUsageTelemetrySampler> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_METRICS_REPORTING_APPS_APP_USAGE_TELEMETRY_SAMPLER_H_
