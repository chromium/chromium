// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/test_support.h"

#include <string>
#include <vector>

#include "base/containers/contains.h"
#include "base/test/scoped_feature_list.h"

namespace ntp {

MockHistoryService::MockHistoryService() = default;

MockHistoryService::~MockHistoryService() = default;

const std::vector<base::test::FeatureRef>& kAllModuleFeatures = {
    ntp_features::kNtpCalendarModule,
    ntp_features::kNtpDriveModule,
    ntp_features::kNtpFeedModule,
    ntp_features::kNtpMostRelevantTabResumptionModule,
    ntp_features::kNtpOutlookCalendarModule,
};

std::vector<base::test::FeatureRef> ComputeDisabledFeaturesList(
    const std::vector<base::test::FeatureRef>& features,
    const std::vector<base::test::FeatureRef>& enabled_features) {
  std::vector<base::test::FeatureRef> disabled_features;
  std::copy_if(features.begin(), features.end(),
               std::back_inserter(disabled_features),
               [&enabled_features](base::test::FeatureRef feature_to_copy) {
                 return !base::Contains(enabled_features, feature_to_copy);
               });
  return disabled_features;
}

}  // namespace ntp
