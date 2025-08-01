// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/permissions/test/enums_to_string.h"

#include "base/containers/fixed_flat_map.h"

namespace test {

std::string_view ToString(
    permissions::PermissionPredictionSource prediction_source) {
  using PredictionSource =
      permissions::PermissionPredictionSource;

  static constexpr auto map =
      base::MakeFixedFlatMap<PredictionSource, std::string_view>(
          {{PredictionSource::kServerSideCpssV3Model, "ServerSideCpssV3Model"},
           {PredictionSource::kOnDeviceAiv1AndServerSideModel,
            "OnDeviceAiv1AndServerSideModel"},
           {PredictionSource::kOnDeviceAiv3AndServerSideModel,
            "OnDeviceAiv3AndServerSideModel"},
           {PredictionSource::kOnDeviceCpssV1Model, "OnDeviceCpssV1Model"},
           {PredictionSource::kNoCpssModel, "NoCpssMode"}});

  auto it = map.find(prediction_source);
  return (it == map.end()) ? "Unknown" : it->second;
}

}  // namespace test
