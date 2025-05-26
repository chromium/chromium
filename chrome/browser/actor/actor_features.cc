// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/actor_features.h"

#include <string>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace actor {

BASE_FEATURE(kGlicActionAllowlist,
             "GlicActionAllowlist",
             base::FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE_PARAM(std::string,
                   kAllowlist,
                   &kGlicActionAllowlist,
                   "allowlist",
                   "");
BASE_FEATURE_PARAM(std::string,
                   kAllowlistExact,
                   &kGlicActionAllowlist,
                   "allowlist_exact",
                   "");
BASE_FEATURE_PARAM(bool,
                   kAllowlistOnly,
                   &kGlicActionAllowlist,
                   "allowlist_only",
                   true);

BASE_FEATURE(kGlicActionUseOptimizationGuide,
             "GlicActionUseOptimizationGuide",
             base::FEATURE_ENABLED_BY_DEFAULT);

}  // namespace actor
