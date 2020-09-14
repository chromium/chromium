// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SUBRESOURCE_FILTER_ADS_INTERVENTION_MANAGER_H_
#define CHROME_BROWSER_SUBRESOURCE_FILTER_ADS_INTERVENTION_MANAGER_H_

#include <memory>

#include "base/optional.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/core/mojom/subresource_filter.mojom.h"

class GURL;

namespace base {
class Clock;
}

// This class tracks ads interventions that have occurred on origins and is
// bound to the user's profile. The ads intervention manager operates in two
// modes set by the feature flag kAdsInterventionsEnforced:
// 1. Dry run: Ads are not blocked on sites with ad interventions, however,
//    the ads intervention manager records metrics as if ads were blocked.
//    If the ads intervention manager is asked to intervene on the same URL
//    in the period where we would block ads during enforcement, it will only
//    record the first seen intervention.
// 2. Enforced: Ads are blocked on sites with ad interventions.
//
// The duration of an ad intervention is set by the feature flag
// kAdsInterventionDuration.
//
// This class maintain's metadata for ads interventions in the user's website
// settings. This is persisted to disk and cleared with browsing history. The
// content subresource filter manager expires ads intervention metadata after
// 7 days. As a result, kAdsInterventionDuration should be less than 7 days
// to prevent expiry from impacting metrics. The metadata is scoped to each
// url's origin. This API would ideally work with Origins insead of GURLs,
// however, downstream APIs use GURL's.
class AdsInterventionManager {
 public:
  // Struct representing the last triggered ads intervention.
  struct LastAdsIntervention {
    base::TimeDelta duration_since;
    subresource_filter::mojom::AdsViolation ads_violation;
  };

  // The content_settings_manager should outlive the ads intervention manager.
  // This is satisfied as the SubresourceFilterContentSettingsManager and the
  // AdsInterventionManager are both bound to the profile.
  explicit AdsInterventionManager(
      SubresourceFilterContentSettingsManager* content_settings_manager);
  ~AdsInterventionManager();
  AdsInterventionManager(const AdsInterventionManager&) = delete;
  AdsInterventionManager& operator=(const AdsInterventionManager&) = delete;

  // The ads intervention manager should trigger an ads intervention on each
  // subsequent page load to |url| for kAdsInterventionDuration. The active
  // intervention is recorded in the user's website settings and updates
  // |url| site metadata with the last active intervention.
  void TriggerAdsInterventionForUrlOnSubsequentLoads(
      const GURL& url,
      subresource_filter::mojom::AdsViolation ads_violation);

  // Returns the last active ads intervention written to metadata,
  // otherwise base::nullopt is returned.
  base::Optional<LastAdsIntervention> GetLastAdsIntervention(
      const GURL& url) const;

  void set_clock_for_testing(base::Clock* clock) { clock_ = clock; }

 private:
  // The SubresourceFilterContentSettingsManager is guaranteed to outlive the
  // AdsInterventionManager. Both are bound to the profile.
  SubresourceFilterContentSettingsManager* settings_manager_ = nullptr;

  base::Clock* clock_;
};

#endif  // CHROME_BROWSER_SUBRESOURCE_FILTER_ADS_INTERVENTION_MANAGER_H_
