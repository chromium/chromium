// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/chrome_feature_flags/arc_feature_string_builder.h"

#include "base/metrics/field_trial_params.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"

namespace arc {

ArcFeatureStringBuilder::ArcFeatureStringBuilder() = default;
ArcFeatureStringBuilder::~ArcFeatureStringBuilder() = default;

void ArcFeatureStringBuilder::AddFeature(const base::Feature& feature) {
  base::FieldTrialParams params;
  // `GetFieldTrialParamsByFeature` will not modify `params` if the feature is
  // disabled or `feature` doesn't have any associated param.
  base::GetFieldTrialParamsByFeature(feature, &params);
  feature_params_.emplace(feature.name, params);
}

std::string ArcFeatureStringBuilder::ToString() const {
  std::vector<std::string> feature_and_params;
  for (const auto& [name, params] : feature_params_) {
    if (params.empty()) {
      feature_and_params.emplace_back(name);
    } else {
      std::vector<std::string> paramslist;
      for (const auto& [key, value] : params) {
        paramslist.emplace_back(key);
        paramslist.emplace_back(value);
      }
      feature_and_params.emplace_back(
          base::StrCat({name, ":", base::JoinString(paramslist, "/")}));
    }
  }
  return base::JoinString(feature_and_params, ",");
}

}  // namespace arc
