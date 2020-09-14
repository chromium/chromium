// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/subresource_filter/ads_intervention_manager.h"

#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/subresource_filter/subresource_filter_content_settings_manager.h"
#include "components/subresource_filter/core/browser/subresource_filter_features.h"
#include "url/gurl.h"

namespace {

// Key into the website settings dict for last active ads violation.
const char kLastAdsViolationTimeKey[] = "LastAdsViolationTime";
const char kLastAdsViolationKey[] = "LastAdsViolation";

}  // namespace

using subresource_filter::mojom::AdsViolation;

AdsInterventionManager::AdsInterventionManager(
    SubresourceFilterContentSettingsManager* settings_manager)
    : settings_manager_(settings_manager),
      clock_(base::DefaultClock::GetInstance()) {}

AdsInterventionManager::~AdsInterventionManager() = default;

void AdsInterventionManager::TriggerAdsInterventionForUrlOnSubsequentLoads(
    const GURL& url,
    AdsViolation ads_violation) {
  std::unique_ptr<base::DictionaryValue> additional_metadata =
      std::make_unique<base::DictionaryValue>();

  double now = clock_->Now().ToDoubleT();
  additional_metadata->SetDouble(kLastAdsViolationTimeKey, now);
  additional_metadata->SetInteger(kLastAdsViolationKey,
                                  static_cast<int>(ads_violation));

  bool activated = base::FeatureList::IsEnabled(
      subresource_filter::kAdsInterventionsEnforced);
  // This is a no-op if the metadata already exists for an active
  // ads intervention.
  settings_manager_->SetSiteMetadataBasedOnActivation(
      url, activated,
      SubresourceFilterContentSettingsManager::ActivationSource::
          kAdsIntervention,
      std::move(additional_metadata));
}

base::Optional<AdsInterventionManager::LastAdsIntervention>
AdsInterventionManager::GetLastAdsIntervention(const GURL& url) const {
  int ads_violation;
  double last_violation_time;
  // The last active ads intervention is stored in the site metadata.
  std::unique_ptr<base::DictionaryValue> dict =
      settings_manager_->GetSiteMetadata(url);

  if (dict && dict->GetInteger(kLastAdsViolationKey, &ads_violation) &&
      dict->GetDouble(kLastAdsViolationTimeKey, &last_violation_time)) {
    base::TimeDelta diff =
        clock_->Now() - base::Time::FromDoubleT(last_violation_time);

    return LastAdsIntervention(
        {diff, static_cast<AdsViolation>(ads_violation)});
  }

  return base::nullopt;
}
