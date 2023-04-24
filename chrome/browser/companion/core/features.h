// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_FEATURES_H_
#define CHROME_BROWSER_COMPANION_CORE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace companion {
namespace features {

BASE_DECLARE_FEATURE(kSidePanelCompanion);
extern const base::FeatureParam<std::string> kHomepageURLForCompanion;
extern const base::FeatureParam<std::string> kImageUploadURLForCompanion;
extern const base::FeatureParam<bool> kEnableOpenCompanionForImageSearch;
extern const base::FeatureParam<bool> kEnableOpenCompanionForWebSearch;

}  // namespace features

namespace switches {
extern const char kDisableCheckUserPermissionsForCompanion[];

// Returns true if checking of the user's permissions to share page information
// with the Companion server should be ignored. Returns true only in tests.
bool ShouldOverrideCheckingUserPermissionsForCompanion();

}  // namespace switches
}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_FEATURES_H_
