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

// Returns whether Sunfish-related UI can be shown. This function checks the
// Sunfish-feature flag, Sunfish prefs and Sunfish policy.
bool ASH_EXPORT CanShowSunfishUi();

// Returns whether Sunfish-related UI or Scanner-related UI can be shown.
// This is also the source of truth for:
// - whether a capture mode session with `kSunfish` behavior type is allowed.
// - whether any action buttons can be shown in the default capture mode
//   session.
//
// This function checks whether `CanShowSunfishUi` or
// `ScannerController::CanShowUiForShell` are true.
bool ASH_EXPORT CanShowSunfishOrScannerUi();

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_CAPTURE_MODE_CAPTURE_MODE_API_H_
