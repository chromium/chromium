// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/power/ml/smart_dim/ml_agent_util.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace power {
namespace ml {

namespace {

// Extracts node id and inserts into the name_2_node_map.
// Both input and output should only contain 1 node id.
bool PopulateMapFromNamesAndNodes(
    const std::vector<std::string>& names,
    const base::Value& nodes,
    base::flat_map<std::string, int>* name_2_node_map) {
  if (names.size() != 1) {
    DVLOG(1) << "names should contain only 1 string element.";
    return false;
  }

  if (nodes.GetList().size() != 1 || !nodes.GetList()[0].is_int()) {
    DVLOG(1) << "nodes should contain only 1 integer element.";
    return false;
  }

  name_2_node_map->emplace(names[0], nodes.GetList()[0].GetInt());
  return true;
}

}  // namespace

constexpr char kSmartDimInputNodeName[] = "input";
constexpr char kSmartDimOutputNodeName[] = "output";

bool ParseMetaInfoFromJsonObject(const base::Value& root,
                                 std::string* metrics_model_name,
                                 double* dim_threshold,
                                 size_t* expected_feature_size,
                                 base::flat_map<std::string, int>* inputs,
                                 base::flat_map<std::string, int>* outputs) {
  DCHECK(metrics_model_name && dim_threshold && expected_feature_size &&
         inputs && outputs);

  const std::string* metrics_model_name_value =
      root.FindStringKey("metrics_model_name");
  const absl::optional<double> dim_threshold_value =
      root.FindDoubleKey("threshold");
  const absl::optional<int> expected_feature_size_value =
      root.FindIntKey("expected_feature_size");

  if (!metrics_model_name_value || *metrics_model_name_value == "" ||
      dim_threshold_value == absl::nullopt ||
      expected_feature_size_value == absl::nullopt) {
    DVLOG(1) << "metadata_json missing expected field(s).";
    return false;
  }

  *metrics_model_name = *metrics_model_name_value;
  *dim_threshold = dim_threshold_value.value();
  *expected_feature_size =
      static_cast<size_t>(expected_feature_size_value.value());

  const base::Value* input_nodes = root.FindListKey("input_nodes");
  const base::Value* output_nodes = root.FindListKey("output_nodes");

  if (!input_nodes || !output_nodes ||
      !PopulateMapFromNamesAndNodes({kSmartDimInputNodeName}, *input_nodes,
                                    inputs) ||
      !PopulateMapFromNamesAndNodes({kSmartDimOutputNodeName}, *output_nodes,
                                    outputs)) {
    DVLOG(1) << "Failed to load inputs and outputs maps from metadata_json";
    *metrics_model_name = "";
    *dim_threshold = 0.0;
    *expected_feature_size = 0;
    inputs->clear();
    outputs->clear();
    return false;
  }

  return true;
}

}  // namespace ml
}  // namespace power
}  // namespace ash
