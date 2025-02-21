// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_API_H_
#define ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_API_H_

#include "ash/ash_export.h"

namespace ash {

// Full screen capture for each available display if no restricted content
// exists on that display, each capture is saved as an individual file.
// Note: this won't start a capture mode session.
void ASH_EXPORT CaptureScreenshotsOfAllDisplays();

// Returns true if the active account can bypass the feature key check.
bool ASH_EXPORT IsSunfishFeatureEnabledWithFeatureKey();

// Returns whether a capture mode session with `kSunfish` behavior type is
// is allowed by feature flags and user prefs / policies.
// This is also the source of truth for whether any action buttons can be shown
// in the default capture mode session.
//
// This function checks:
// - whether the Sunfish-feature flag is enabled OR Scanner-related UI is
//   allowed to be shown, AND
// - additional checks for Sunfish prefs and Sunfish policy.
//
// As Sunfish prefs and policy are checked even if Scanner's UI can be shown,
// the Scanner and Sunfish features are coupled together.
// TODO: crbug.com/397521940 - Remove the additional checks when Scanner's UI
// can be shown, decoupling the two features.
// TODO: crbug.com/397784915 - Rename this function to be more understandable.
bool ASH_EXPORT IsSunfishSessionAllowed();

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_API_H_
