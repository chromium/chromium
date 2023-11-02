// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_ML_AGENT_UTIL_H_
#define CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_ML_AGENT_UTIL_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/values.h"

namespace ash {
namespace power {
namespace ml {

// Note: these names doesn't need to respect the real node names in TF graph.
// But we must use the same input and output node names in loading model and
// composing/extracting input/output tensors.
extern const char kSmartDimInputNodeName[];
extern const char kSmartDimOutputNodeName[];

// Extract essential fields from parsed JSON dict.
bool ParseMetaInfoFromJsonObject(const base::Value& root,
                                 std::string* metrics_model_name,
                                 double* dim_threshold,
                                 size_t* expected_feature_size,
                                 base::flat_map<std::string, int>* inputs,
                                 base::flat_map<std::string, int>* outputs);

}  // namespace ml
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_ML_SMART_DIM_ML_AGENT_UTIL_H_
