// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_rust_shim.h"

#include <string_view>

#include "base/metrics/field_trial_params.h"

namespace base {

bool GetFieldTrialParamByFeatureAsBoolShim(const Feature& feature,
                                           ::rust::Str param_name,
                                           bool default_value) {
  return GetFieldTrialParamByFeatureAsBool(feature, std::string(param_name),
                                           default_value);
}

int32_t GetFieldTrialParamByFeatureAsIntShim(const Feature& feature,
                                             ::rust::Str param_name,
                                             int32_t default_value) {
  return GetFieldTrialParamByFeatureAsInt(feature, std::string(param_name),
                                          default_value);
}

}  // namespace base
