// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UI_ASSISTANT_UI_CONSTANTS_H_
#define ASH_ASSISTANT_UI_ASSISTANT_UI_CONSTANTS_H_

#include "base/component_export.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/gfx/color_palette.h"

namespace gfx {
class FontList;
}  // namespace gfx

namespace ash {

// Appearance.
// TODO(b/233386078): Usage of kPreferredWidthDip is discouraged as it can
// change
constexpr int kPreferredWidthDip = 640;
constexpr int kSpacingDip = 8;
constexpr int kMarginDip = 8;

// Typography.
constexpr SkColor kTextColorPrimary = gfx::kGoogleGrey900;
constexpr SkColor kTextColorSecondary = gfx::kGoogleGrey700;

// TODO(dmblack): Move the other constants into ash::assistant::ui.
namespace assistant {
namespace ui {

// Expected launcher margin is 24. But AppListBubbleAssistantPage is shifted by
// 1px, i.e. has 1px margin. See b/233384263 for details.
constexpr int kHorizontalMargin = 23;
constexpr int kHorizontalPadding = 20;

// Window property to instruct the event targeter for the Assistant window to
// only allow mouse click events to reach the specified |window|. All other
// events will not be explored by |window|'s subtree for handling.
COMPONENT_EXPORT(ASSISTANT_UI_CONSTANTS)
extern const aura::WindowProperty<bool>* const kOnlyAllowMouseClickEvents;

// Returns the default font list for Assistant UI.
COMPONENT_EXPORT(ASSISTANT_UI_CONSTANTS)
const gfx::FontList& GetDefaultFontList();

// The maximum number of user sessions in which to show Assistant onboarding.
constexpr int kOnboardingMaxSessionsShown = 3;

}  // namespace ui
}  // namespace assistant

}  // namespace ash

#endif  // ASH_ASSISTANT_UI_ASSISTANT_UI_CONSTANTS_H_
