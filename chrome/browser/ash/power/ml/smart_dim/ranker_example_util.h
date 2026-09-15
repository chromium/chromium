// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_RANKER_EXAMPLE_UTIL_H_
#define CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_RANKER_EXAMPLE_UTIL_H_

#include <string>

#include "chrome/browser/ash/power/ml/smart_dim/ranker_example.pb.h"

namespace assist_ranker {

// If |key| feature is found in |example|, fills in |feature| and return true.
// Returns false if the feature is not found. |feature| can be nullptr. In such
// a case, the return value is not changed, but |feature| will not be filled in.
// This can be used to check for the presence of a key.
[[nodiscard]] bool SafeGetFeature(const std::string& key,
                                  const RankerExample& example,
                                  Feature* feature);

// Extract value from |feature| for scalar feature types. Returns true and fills
// in |value| if the feature is found and has a float, int32 or bool value.
// Returns false otherwise.
[[nodiscard]] bool GetFeatureValueAsFloat(const std::string& key,
                                          const RankerExample& example,
                                          float* value);

}  // namespace assist_ranker

#endif  // CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_RANKER_EXAMPLE_UTIL_H_
