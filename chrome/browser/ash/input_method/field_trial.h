// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_FIELD_TRIAL_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_FIELD_TRIAL_H_

#include <string>

#include "base/feature_list.h"

namespace ash {
namespace input_method {

enum class ParamName { kDenylist };

std::string GetFieldTrialParam(const base::Feature& feature,
                               const ParamName& param_name);

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_FIELD_TRIAL_H_
