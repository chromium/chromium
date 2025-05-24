// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_FEATURES_H_
#define CHROME_BROWSER_ACTOR_ACTOR_FEATURES_H_

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace actor {

BASE_DECLARE_FEATURE(kGlicActionAllowlist);

BASE_DECLARE_FEATURE_PARAM(std::string, kAllowlist);
BASE_DECLARE_FEATURE_PARAM(std::string, kAllowlistExact);
BASE_DECLARE_FEATURE_PARAM(bool, kAllowlistOnly);

BASE_DECLARE_FEATURE(kGlicActionUseOptimizationGuide);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_FEATURES_H_
