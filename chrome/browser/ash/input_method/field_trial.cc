// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/input_method/field_trial.h"

#include "base/metrics/field_trial_params.h"

namespace ash {
namespace input_method {

std::string ToString(const ParamName& param_name) {
  switch (param_name) {
    case ParamName::kDenylist:
      return "block";
  }
}

std::string GetFieldTrialParam(const base::Feature& feature,
                               const ParamName& param_name) {
  return base::GetFieldTrialParamValueByFeature(feature, ToString(param_name));
}

}  // namespace input_method
}  // namespace ash
