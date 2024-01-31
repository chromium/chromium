// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/window_predictor/window_predictor_utils.h"

#include "base/strings/string_util.h"
#include "chromeos/ui/base/window_state_type.h"
#include "components/app_restore/app_restore_data.h"

namespace arc {

namespace {
const char kPlayStorePrefix[] = "com.android.vending";
const char kGoogleOfficialPrefix1[] = "com.android.google";
const char kGoogleOfficialPrefix2[] = "com.google.android";
}  // namespace

bool CanLaunchGhostWindowByRestoreData(
    const app_restore::AppRestoreData& restore_data) {
  const bool not_need_bounds =
      restore_data.window_state_type == chromeos::WindowStateType::kMaximized ||
      restore_data.window_state_type == chromeos::WindowStateType::kFullscreen;
  return not_need_bounds || restore_data.bounds_in_root.has_value() ||
         restore_data.current_bounds.has_value();
}

bool IsGoogleSeriesPackage(const std::string& package_name) {
  return base::StartsWith(package_name, kPlayStorePrefix) ||
         base::StartsWith(package_name, kGoogleOfficialPrefix1) ||
         base::StartsWith(package_name, kGoogleOfficialPrefix2);
}

}  // namespace arc
