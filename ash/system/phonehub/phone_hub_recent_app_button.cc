// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_hub_recent_app_button.h"

#include <utility>

#include "ash/style/ash_color_provider.h"
#include "ash/style/style_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/controls/focus_ring.h"
#include "ui/views/controls/highlight_path_generator.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

// Appearance in DIPs.
constexpr int kRecentAppButtonSize = 42;

}  // namespace

PhoneHubRecentAppButton::PhoneHubRecentAppButton(
    const gfx::Image& icon,
    const std::u16string& visible_app_name,
    PressedCallback callback)
    : views::ImageButton(std::move(callback)) {
  SetImageModel(
      views::Button::STATE_NORMAL,
      ui::ImageModel::FromImageSkia(
          gfx::ImageSkiaOperations::CreateResizedImage(
              icon.AsImageSkia(), skia::ImageOperations::RESIZE_BEST,
              gfx::Size(kRecentAppButtonSize, kRecentAppButtonSize))));
  SetImageHorizontalAlignment(ALIGN_CENTER);
  SetImageVerticalAlignment(ALIGN_MIDDLE);
  StyleUtil::SetUpInkDropForButton(this);
  views::InstallCircleHighlightPathGenerator(this);
  GetViewAccessibility().SetName(visible_app_name);
  SetTooltipText(visible_app_name);
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
}

PhoneHubRecentAppButton::~PhoneHubRecentAppButton() = default;

gfx::Size PhoneHubRecentAppButton::CalculatePreferredSize(
    const views::SizeBounds& available_size) const {
  return gfx::Size(kRecentAppButtonSize, kRecentAppButtonSize);
}

void PhoneHubRecentAppButton::PaintButtonContents(gfx::Canvas* canvas) {
  cc::PaintFlags flags;
  flags.setAntiAlias(true);
  views::ImageButton::PaintButtonContents(canvas);
}

BEGIN_METADATA(PhoneHubRecentAppButton)
END_METADATA

}  // namespace ash
