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
  const app_restore::WindowInfo& window_info = restore_data.window_info;
  const bool not_need_bounds =
      window_info.window_state_type == chromeos::WindowStateType::kMaximized ||
      window_info.window_state_type == chromeos::WindowStateType::kFullscreen;
  if (not_need_bounds || window_info.current_bounds.has_value()) {
    return true;
  }

  return window_info.arc_extra_info &&
         window_info.arc_extra_info->bounds_in_root.has_value();
}

bool IsGoogleSeriesPackage(const std::string& package_name) {
  return base::StartsWith(package_name, kPlayStorePrefix) ||
         base::StartsWith(package_name, kGoogleOfficialPrefix1) ||
         base::StartsWith(package_name, kGoogleOfficialPrefix2);
}

}  // namespace arc
