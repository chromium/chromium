// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_TELEMETRY_SAMPLER_H_
#define CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_TELEMETRY_SAMPLER_H_

#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/thread_annotations.h"
#include "chrome/browser/profiles/profile.h"
#include "components/reporting/metrics/sampler.h"
#include "components/reporting/proto/synced/metric_data.pb.h"

namespace reporting {

// Sampler used to collect website usage telemetry data from the user profile
// pref store. This usage data is originally persisted in the pref store by the
// `WebsiteUsageObserver` as it observes and tracks data collection from the
// `WebsiteMetrics` component.
class WebsiteUsageTelemetrySampler : public Sampler {
 public:
  explicit WebsiteUsageTelemetrySampler(base::WeakPtr<Profile> profile);
  WebsiteUsageTelemetrySampler(const WebsiteUsageTelemetrySampler& other) =
      delete;
  WebsiteUsageTelemetrySampler& operator=(
      const WebsiteUsageTelemetrySampler& other) = delete;
  ~WebsiteUsageTelemetrySampler() override;

  // Collects website usage telemetry data from the user pref store across
  // several URLs and triggers the specified callback with batched usage data.
  // Sampler:
  void MaybeCollect(OptionalMetricCallback callback) override;

 private:
  // Deletes website usage data from the user pref store. Triggered only after
  // website usage telemetry data has been consumed to prevent data loss.
  void DeleteWebsiteUsageDataFromPrefStore(
      const WebsiteUsageData* website_usage_data);

  SEQUENCE_CHECKER(sequence_checker_);

  // Weak pointer to the user profile. Used to access the user pref store.
  const base::WeakPtr<Profile> profile_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<WebsiteUsageTelemetrySampler> weak_ptr_factory_{this};
};

}  // namespace reporting

#endif  // CHROME_BROWSER_CHROMEOS_REPORTING_WEBSITES_WEBSITE_USAGE_TELEMETRY_SAMPLER_H_
