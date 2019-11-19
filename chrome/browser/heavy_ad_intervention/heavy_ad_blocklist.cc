// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/heavy_ad_intervention/heavy_ad_blocklist.h"

#include <string>
#include <utility>

#include "base/metrics/field_trial_params.h"
#include "chrome/common/chrome_features.h"
#include "components/blacklist/opt_out_blacklist/opt_out_store.h"

namespace {

const char kHostDurationHours[] = "host-duration-hours";
const char kHostThreshold[] = "host-threshold";
const char kHostsInMemory[] = "hosts-in-memory";

const char kTypeVersion[] = "type-version";

int GetBlocklistParamValue(const std::string& param, int default_value) {
  return base::GetFieldTrialParamByFeatureAsInt(
      features::kHeavyAdPrivacyMitigations, param, default_value);
}

}  // namespace

HeavyAdBlocklist::HeavyAdBlocklist(
    std::unique_ptr<blacklist::OptOutStore> opt_out_store,
    base::Clock* clock,
    blacklist::OptOutBlacklistDelegate* blacklist_delegate)
    : OptOutBlacklist(std::move(opt_out_store), clock, blacklist_delegate) {
  Init();
}

HeavyAdBlocklist::~HeavyAdBlocklist() = default;

bool HeavyAdBlocklist::ShouldUseSessionPolicy(base::TimeDelta* duration,
                                              size_t* history,
                                              int* threshold) const {
  return false;
}

bool HeavyAdBlocklist::ShouldUsePersistentPolicy(base::TimeDelta* duration,
                                                 size_t* history,
                                                 int* threshold) const {
  return false;
}

bool HeavyAdBlocklist::ShouldUseHostPolicy(base::TimeDelta* duration,
                                           size_t* history,
                                           int* threshold,
                                           size_t* max_hosts) const {
  const int kDefaultHostsInMemory = 50;
  const int kDefaultHostDurationHours = 24;
  const int kDefaultHostThreshold = 5;
  *max_hosts = GetBlocklistParamValue(kHostsInMemory, kDefaultHostsInMemory);
  *duration = base::TimeDelta::FromHours(
      GetBlocklistParamValue(kHostDurationHours, kDefaultHostDurationHours));
  *history = GetBlocklistParamValue(kHostThreshold, kDefaultHostThreshold);
  *threshold = GetBlocklistParamValue(kHostThreshold, kDefaultHostThreshold);
  return true;
}

bool HeavyAdBlocklist::ShouldUseTypePolicy(base::TimeDelta* duration,
                                           size_t* history,
                                           int* threshold) const {
  return false;
}

blacklist::BlacklistData::AllowedTypesAndVersions
HeavyAdBlocklist::GetAllowedTypes() const {
  return {{static_cast<int>(HeavyAdBlocklistType::kHeavyAdOnlyType),
           GetBlocklistParamValue(kTypeVersion, 0)}};
}
