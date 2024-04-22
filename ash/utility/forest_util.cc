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

namespace {

// Caches the result of `switches::IsForestSecretKeyMatched()` which not only
// has to fetch and compare strings, but will log an error everytime if they do
// not match.
std::optional<bool> g_cache_secret_key_matched;

}

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

  if (!g_cache_secret_key_matched) {
    g_cache_secret_key_matched = switches::IsForestSecretKeyMatched();
  }

  return *g_cache_secret_key_matched;
}

}  // namespace ash
