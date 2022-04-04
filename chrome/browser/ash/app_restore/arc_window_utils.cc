// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/arc_window_utils.h"

#include "ash/components/arc/arc_util.h"
#include "chrome/browser/ash/app_restore/full_restore_prefs.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "components/app_restore/features.h"
#include "components/exo/wm_helper.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user_manager.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/views/window/caption_button_layout_constants.h"

namespace {

void ScaleToRoundedRectWithHeightInsets(apps::mojom::Rect* rect,
                                        double scale_factor,
                                        int height) {
  if (rect == nullptr)
    return;

  gfx::Rect bounds = gfx::Rect(rect->x, rect->y, rect->width, rect->height);
  if (height)
    bounds.Inset(gfx::Insets::TLBR(height, 0, 0, 0));
  auto res_rect = gfx::ScaleToRoundedRect(bounds, scale_factor);
  rect->x = res_rect.x();
  rect->y = res_rect.y();
  rect->width = res_rect.width();
  rect->height = res_rect.height();
}

}  // namespace

namespace ash {
namespace full_restore {

bool IsArcGhostWindowEnabled() {
  if (!::full_restore::features::IsArcGhostWindowEnabled() ||
      !exo::WMHelper::HasInstance()) {
    return false;
  }

  // Returens false if the feature not enabled on ARC P specifically.
  if (!arc::IsArcVmEnabled() &&
      !base::FeatureList::IsEnabled(features::kArcPiGhostWindow)) {
    return false;
  }

  auto* user_manager = user_manager::UserManager::Get();
  // Check `user_manager`, which might be null for test cases.
  if (!user_manager || !user_manager->GetPrimaryUser())
    return true;

  Profile* profile = ProfileHelper::Get()->GetProfileByAccountId(
      user_manager->GetPrimaryUser()->GetAccountId());
  DCHECK(profile);
  return profile->GetPrefs()->GetBoolean(kGhostWindowEnabled);
}

absl::optional<double> GetDisplayScaleFactor(int64_t display_id) {
  display::Display display;
  if (display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                            &display)) {
    return display.device_scale_factor();
  }
  return absl::nullopt;
}

apps::mojom::WindowInfoPtr HandleArcWindowInfo(
    apps::mojom::WindowInfoPtr window_info) {
  // Remove ARC bounds info if the ghost window disabled. The bounds will
  // be controlled by ARC.
  if (!IsArcGhostWindowEnabled()) {
    window_info->bounds.reset();
    return window_info;
  }
  auto scale_factor = GetDisplayScaleFactor(window_info->display_id);

  // Remove ARC bounds info if the the display doesn't exist. The bounds will
  // be controlled by ARC.
  if (!scale_factor.has_value()) {
    window_info->bounds.reset();
    return window_info;
  }

  // For ARC P, the window bounds in launch parameters should minus caption
  // height.
  int extra_caption_height = 0;
  if (!arc::IsArcVmEnabled()) {
    extra_caption_height =
        views::GetCaptionButtonLayoutSize(
            views::CaptionButtonLayoutSize::kNonBrowserCaption)
            .height();
  }
  ScaleToRoundedRectWithHeightInsets(
      window_info->bounds.get(), scale_factor.value(), extra_caption_height);
  return window_info;
}

bool IsValidThemeColor(uint32_t theme_color) {
  return SkColorGetA(theme_color) == SK_AlphaOPAQUE;
}

const std::string WindowIdToAppId(int window_id) {
  return std::string("org.chromium.arc.session.") +
         base::NumberToString(window_id);
}

}  // namespace full_restore
}  // namespace ash
