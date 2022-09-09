// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/arc/window_predictor/window_predictor.h"

#include "base/no_destructor.h"
#include "chrome/browser/ash/app_restore/app_launch_handler.h"
#include "chromeos/ui/base/window_state_type.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/point.h"

namespace arc {

namespace {

// Pre-defined screen size for ARC. See ArcLaunchParamsModifier.java in ARC
// codebase.

// Screen size of Nexus 5x
constexpr gfx::Size kDefaultPortraitPhoneSize(412, 732);
constexpr gfx::Size kDefaultLandscapeTabletSize(1064, 600);

// In ARC R and above, the uniform scale factor is applied on ARC window render
// process.
// TODO(sstan): Replace by calculating from real display scale factor.
constexpr float kArcUniformScaleFactor = 1.2;

gfx::Size GetPhoneSize() {
  return ScaleToCeiledSize(kDefaultPortraitPhoneSize, kArcUniformScaleFactor);
}

gfx::Size GetTabletSize() {
  return ScaleToCeiledSize(kDefaultLandscapeTabletSize, kArcUniformScaleFactor);
}

// Get window bounds in the middle of a display in global coordinate.
gfx::Rect GetMiddleBounds(const display::Display& display,
                          const gfx::Size& size) {
  // Shrink size if it larger then display size.
  // TODO(sstan): ARC P and R has different behavior for caption bar, verify if
  // it need consider the caption bar.
  auto shrink_size = size;
  shrink_size.SetToMin(display.work_area_size());

  auto bounds_rect = display.work_area();
  bounds_rect.ClampToCenteredSize(shrink_size);
  return bounds_rect;
}

}  // namespace

// static
WindowPredictor* WindowPredictor::GetInstance() {
  static base::NoDestructor<WindowPredictor> instance;
  return instance.get();
}

WindowPredictor::WindowPredictor() = default;

WindowPredictor::~WindowPredictor() = default;

void WindowPredictor::MaybeCreateAppLaunchHandler(Profile* profile) {
  DCHECK(profile);
  if (app_launch_handler_ && app_launch_handler_->profile() == profile)
    return;

  app_launch_handler_ = std::make_unique<ArcPredictorAppLaunchHandler>(profile);
}

arc::mojom::WindowInfoPtr WindowPredictor::PredictAppWindowInfo(
    const ArcAppListPrefs::AppInfo& app_info,
    arc::mojom::WindowInfoPtr window_info) {
  // TODO(sstan): Consider fallback case.
  // TODO(sstan): Verify the affect from per-display density.
  // TODO(sstan): Consider multi display case.
  if (!window_info)
    return nullptr;
  auto disp = display::Display::GetDefaultDisplay();
  if (window_info->display_id != display::kInvalidDisplayId) {
    display::Screen::GetScreen()->GetDisplayWithDisplayId(
        window_info->display_id, &disp);
  }

  const auto& layout = app_info.initial_window_layout;
  switch (layout.type) {
    case arc::mojom::WindowSizeType::kMaximize:
      window_info->state =
          static_cast<int32_t>(chromeos::WindowStateType::kMaximized);
      break;
    case arc::mojom::WindowSizeType::kTabletSize:
      window_info->state =
          static_cast<int32_t>(chromeos::WindowStateType::kNormal);
      window_info->bounds = GetMiddleBounds(disp, GetTabletSize());
      break;
    case arc::mojom::WindowSizeType::kPhoneSize:
    case arc::mojom::WindowSizeType::kUnknown:
    default:
      window_info->state =
          static_cast<int32_t>(chromeos::WindowStateType::kNormal);
      window_info->bounds = GetMiddleBounds(disp, GetPhoneSize());
  }

  return window_info;
}

}  // namespace arc
