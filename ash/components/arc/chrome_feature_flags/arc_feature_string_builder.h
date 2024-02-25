// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_CHROME_FEATURE_FLAGS_ARC_FEATURE_STRING_BUILDER_H_
#define ASH_COMPONENTS_ARC_CHROME_FEATURE_FLAGS_ARC_FEATURE_STRING_BUILDER_H_

#include <map>
#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace arc {

// This class makes a formatted string that includes feature name and its
// parameters.
class ArcFeatureStringBuilder {
 public:
  ArcFeatureStringBuilder();
  ArcFeatureStringBuilder(const ArcFeatureStringBuilder& other) = delete;
  ArcFeatureStringBuilder& operator=(const ArcFeatureStringBuilder&) = delete;
  ~ArcFeatureStringBuilder();

  // Add feature and parameters to Builder.
  void AddFeature(const base::Feature& feature);

  // Return formatted string that include feature name and its parameters.
  // (i.e. "FeatureA:param_name1/param1/param_name2/param2,FeatureB,FeatureC")
  std::string ToString() const;

 private:
  // The key of the map is feature's name
  std::map<std::string, base::FieldTrialParams> feature_params_;
};

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_CHROME_FEATURE_FLAGS_ARC_FEATURE_STRING_BUILDER_H_
