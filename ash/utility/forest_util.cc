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

bool IsForestFeatureEnabled() {
  if (!base::FeatureList::IsEnabled(features::kForestFeature)) {
    return false;
  }

  // TODO(http://b/333952534): Remove the google api DEPS changes.
  if (gaia::IsGoogleInternalAccountEmail(Shell::Get()
                                             ->session_controller()
                                             ->GetActiveAccountId()
                                             .GetUserEmail())) {
    return true;
  }

  return switches::IsForestSecretKeyMatched();
}

}  // namespace ash
