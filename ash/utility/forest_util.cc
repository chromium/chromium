// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/forest_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "google_apis/gaia/gaia_auth_util.h"

namespace ash {

bool IsForestFeatureFlagEnabled() {
  return base::FeatureList::IsEnabled(features::kForestFeature);
}

bool IsForestFeatureEnabled() {
  if (!IsForestFeatureFlagEnabled()) {
    return false;
  }

  // The shell may not be created in some unit tests.
  Shell* shell = Shell::HasInstance() ? Shell::Get() : nullptr;

  // TODO(http://b/333952534): Remove the google api DEPS changes, and move this
  // function back to ash/constants/ash_features.
  if (shell &&
      gaia::IsGoogleInternalAccountEmail(
          shell->session_controller()->GetActiveAccountId().GetUserEmail())) {
    return true;
  }

  return switches::IsForestSecretKeyMatched();
}

}  // namespace ash
