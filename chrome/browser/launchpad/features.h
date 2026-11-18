// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LAUNCHPAD_FEATURES_H_
#define CHROME_BROWSER_LAUNCHPAD_FEATURES_H_

#include <cstddef>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace launchpad::features {

BASE_DECLARE_FEATURE(kLaunchpad);
BASE_DECLARE_FEATURE_PARAM(size_t, kMaxTasksCacheSize);

}  // namespace launchpad::features

#endif  // CHROME_BROWSER_LAUNCHPAD_FEATURES_H_
