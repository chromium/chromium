// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/login/ui/login_button.h"

#include <utility>

#include "ash/login/ui/views_utils.h"
#include "ash/style/ash_color_provider.h"
#include "base/functional/bind.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/animation/flood_fill_ink_drop_ripple.h"
#include "ui/views/animation/ink_drop.h"
#include "ui/views/animation/ink_drop_highlight.h"
#include "ui/views/controls/focus_ring.h"

namespace ash {

namespace {

// Color of the ink drop ripple.
constexpr SkColor kInkDropRippleColor = SkColorSetARGB(0x0F, 0xFF, 0xFF, 0xFF);

// Color of the ink drop highlight.
constexpr SkColor kInkDropHighlightColor =
    SkColorSetARGB(0x14, 0xFF, 0xFF, 0xFF);

}  // namespace

LoginButton::LoginButton(PressedCallback callback)
    : views::ImageButton(std::move(callback)) {
  SetImageHorizontalAlignment(views::ImageButton::ALIGN_CENTER);
  SetImageVerticalAlignment(views::ImageButton::ALIGN_MIDDLE);
  views::InkDrop::Get(this)->SetMode(views::InkDropHost::InkDropMode::ON);
  SetHasInkDropActionOnClick(true);
  views::InkDrop::Get(this)->SetCreateHighlightCallback(base::BindRepeating(
      [](Button* host) {
        return std::make_unique<views::InkDropHighlight>(
            gfx::SizeF(host->size()), kInkDropHighlightColor);
      },
      this));
  views::InkDrop::Get(this)->SetCreateRippleCallback(base::BindRepeating(
      [](LoginButton* host) -> std::unique_ptr<views::InkDropRipple> {
        const gfx::Point center = host->GetLocalBounds().CenterPoint();
        const int radius = host->GetInkDropRadius();
        gfx::Rect bounds(center.x() - radius, center.y() - radius, radius * 2,
                         radius * 2);

        return std::make_unique<views::FloodFillInkDropRipple>(
            views::InkDrop::Get(host), host->size(),
            host->GetLocalBounds().InsetsFrom(bounds),
            views::InkDrop::Get(host)->GetInkDropCenterBasedOnLastEvent(),
            kInkDropRippleColor, 1.f /*visible_opacity*/);
      },
      this));

  SetInstallFocusRingOnFocus(true);
  views::FocusRing::Get(this)->SetColorId(ui::kColorAshFocusRing);
  login_views_utils::ConfigureRectFocusRingCircleInkDrop(
      this, views::FocusRing::Get(this), std::nullopt);
}

LoginButton::~LoginButton() = default;

int LoginButton::GetInkDropRadius() const {
  return std::min(GetLocalBounds().width(), GetLocalBounds().height()) / 2;
}

BEGIN_METADATA(LoginButton)
END_METADATA

}  // namespace ash
