// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_rust_shim.h"

#include <string>
#include <string_view>

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

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

double GetFieldTrialParamByFeatureAsDoubleShim(const Feature& feature,
                                               ::rust::Str param_name,
                                               double default_value) {
  return GetFieldTrialParamByFeatureAsDouble(feature, std::string(param_name),
                                             default_value);
}

::rust::String GetFieldTrialParamByFeatureAsStringShim(
    const Feature& feature,
    ::rust::Str param_name,
    ::rust::Str default_value) {
  return ::rust::String(GetFieldTrialParamByFeatureAsString(
      feature, std::string(param_name), std::string(default_value)));
}

int64_t GetFieldTrialParamByFeatureAsTimeDeltaInMicrosecondsShim(
    const Feature& feature,
    ::rust::Str param_name,
    int64_t default_value_micros) {
  return GetFieldTrialParamByFeatureAsTimeDelta(
             feature, std::string(param_name),
             base::Microseconds(default_value_micros))
      .InMicroseconds();
}

}  // namespace base
