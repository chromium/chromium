// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMPANION_CORE_FEATURES_H_
#define CHROME_BROWSER_COMPANION_CORE_FEATURES_H_

#include <optional>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace companion {
namespace features {

namespace internal {
BASE_DECLARE_FEATURE(kSidePanelCompanion);
BASE_DECLARE_FEATURE(kSidePanelCompanion2);
BASE_DECLARE_FEATURE(kSidePanelCompanionChromeOS);
BASE_DECLARE_FEATURE(kCompanionEnabledByObservingExpsNavigations);
}  // namespace internal

BASE_DECLARE_FEATURE(kCompanionEnableSearchWebInNewTabContextMenuItem);
BASE_DECLARE_FEATURE(kCompanionEnablePageContent);
}  // namespace features

namespace switches {
extern const char kDisableCheckUserPermissionsForCompanion[];
extern const char kForceCompanionPinnedState[];

// Returns true if checking of the user's permissions to share page information
// with the Companion server should be ignored. Returns true only in tests.
bool ShouldOverrideCheckingUserPermissionsForCompanion();

// Returns whether the Companion pin state should force overridden, regardless
// of prefs or labs state.
std::optional<bool> ShouldForceOverrideCompanionPinState();

}  // namespace switches
}  // namespace companion

#endif  // CHROME_BROWSER_COMPANION_CORE_FEATURES_H_
