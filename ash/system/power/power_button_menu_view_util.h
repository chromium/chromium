// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_POWER_POWER_BUTTON_MENU_VIEW_UTIL_H_
#define ASH_SYSTEM_POWER_POWER_BUTTON_MENU_VIEW_UTIL_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/style/color_provider.h"
#include "ash/style/ash_color_id.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/transform.h"
#include "ui/views/highlight_border.h"

namespace ash {

// The duration of showing or dismissing power button menu animation.
constexpr base::TimeDelta kPowerButtonMenuAnimationDuration =
    base::Milliseconds(250);

// Distance of the menu animation transform.
constexpr int kPowerButtonMenuTransformDistanceDp = 16;

// The rounded corner radius of menu.
constexpr int kPowerButtonMenuCornerRadius = 16;

// The border highlight type for the container.
constexpr auto kPowerButtonMenuBorderType =
    views::HighlightBorder::Type::kHighlightBorder1;

// The color id for widget background.
constexpr auto kPowerButtonMenuBackgroundColorId = kColorAshShieldAndBase80;

void SetLayerAnimation(ui::Layer* layer,
                       ui::ImplicitAnimationObserver* observer,
                       bool show,
                       const gfx::Transform& transform);

}  // namespace ash

#endif  // ASH_SYSTEM_POWER_POWER_BUTTON_MENU_VIEW_UTIL_H_
