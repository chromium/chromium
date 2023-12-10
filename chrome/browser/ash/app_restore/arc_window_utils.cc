// Copyright 2021 The Chromium Authors
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

void ScaleToRoundedRectWithHeightInsets(std::optional<gfx::Rect>& rect,
                                        double scale_factor,
                                        int height) {
  if (!rect.has_value())
    return;

  gfx::Rect bounds =
      gfx::Rect(rect->x(), rect->y(), rect->width(), rect->height());
  if (height)
    bounds.Inset(gfx::Insets::TLBR(height, 0, 0, 0));
  rect = gfx::ScaleToRoundedRect(bounds, scale_factor);
}

}  // namespace

namespace ash {
namespace full_restore {

bool IsArcGhostWindowEnabled() {
  if (!exo::WMHelper::HasInstance()) {
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

std::optional<double> GetDisplayScaleFactor(int64_t display_id) {
  // The `kDefaultDisplayId` should not be a valid parameter. Here replace it to
  // primary display id to keep it as the same semantics with Android, since the
  // ARC app window will not be shown on chromium default display (placeholder
  // display when no display connected).
  if (display_id == display::kDefaultDisplayId)
    display_id = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::Display display;
  if (display::Screen::GetScreen()->GetDisplayWithDisplayId(display_id,
                                                            &display)) {
    return display.device_scale_factor();
  }
  return std::nullopt;
}

apps::WindowInfoPtr HandleArcWindowInfo(apps::WindowInfoPtr window_info) {
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
  if (arc::GetArcAndroidSdkVersionAsInt() == arc::kArcVersionP) {
    extra_caption_height =
        views::GetCaptionButtonLayoutSize(
            views::CaptionButtonLayoutSize::kNonBrowserCaption)
            .height();
  }
  ScaleToRoundedRectWithHeightInsets(window_info->bounds, scale_factor.value(),
                                     extra_caption_height);
  return window_info;
}

bool IsValidThemeColor(uint32_t theme_color) {
  return SkColorGetA(theme_color) == SK_AlphaOPAQUE;
}

const std::string WrapSessionAppIdFromWindowId(int window_id) {
  return std::string("org.chromium.arc.session.") +
         base::NumberToString(window_id);
}

}  // namespace full_restore
}  // namespace ash
