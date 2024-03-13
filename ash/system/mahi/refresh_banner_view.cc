// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/mahi/refresh_banner_view.h"
#include <string>

#include "ash/strings/grit/ash_strings.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/mahi/mahi_constants.h"
#include "base/strings/string_util.h"
#include "base/time/time.h"
#include "chromeos/components/mahi/public/cpp/mahi_manager.h"
#include "components/vector_icons/vector_icons.h"
#include "third_party/skia/include/core/SkPath.h"
#include "third_party/skia/include/core/SkPathBuilder.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/views/background.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/layout/layout_types.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr int kRefreshBannerCornerRadius = 20;
constexpr base::TimeDelta kRefreshBannerAnimationDurationMs =
    base::Milliseconds(100);
constexpr gfx::Insets kRefreshBannerInteriorMargin =
    gfx::Insets::TLBR(4, 28, mahi_constants::kRefreshBannerStackDepth + 4, 28);
constexpr gfx::Insets kTitleLabelMargin = gfx::Insets::TLBR(0, 0, 0, 8);

SkPath GetClipPath(gfx::Size size) {
  int width = size.width();
  int height = size.height();

  auto top_left = SkPoint::Make(0, 0);
  auto top_right = SkPoint::Make(width, 0);
  auto bottom_left = SkPoint::Make(0, height);
  auto bottom_right = SkPoint::Make(width, height);
  int radius = kRefreshBannerCornerRadius;
  int bottom_radius = mahi_constants::kPanelCornerRadius;

  const auto horizontal_offset = SkPoint::Make(radius, 0.f);
  const auto vertical_offset = SkPoint::Make(0.f, radius);
  const auto bottom_vertical_offset =
      SkPoint::Make(0.f, mahi_constants::kRefreshBannerStackDepth - 1);
  const auto bottom_horizontal_offset = SkPoint::Make(bottom_radius, 0.f);

  return SkPathBuilder()
      // Start just before the curve of the top-left corner.
      .moveTo(radius, 0.f)
      // Draw the top-left rounded corner.
      .arcTo(top_left, top_left + vertical_offset, radius)
      // Draw the bottom-left rounded corner and the vertical line
      // connecting it to the top-left corner.
      .lineTo(bottom_left)
      .arcTo(bottom_left - bottom_vertical_offset,
             bottom_left - bottom_vertical_offset + bottom_horizontal_offset,
             radius)
      // Draw the bottom-right rounded corner and the horizontal line
      // connecting it to the bottom-left corner.
      .arcTo(bottom_right - bottom_vertical_offset, bottom_right, bottom_radius)
      .lineTo(bottom_right)
      .arcTo(top_right, top_right - horizontal_offset, bottom_radius)
      .lineTo(radius, 0.f)
      .close()
      .detach();
}

}  // namespace

RefreshBannerView::RefreshBannerView() {
  auto* manager = chromeos::MahiManager::Get();

  SetBackground(views::CreateThemedRoundedRectBackground(
      cros_tokens::kCrosSysSystemPrimaryContainer, /*radius=*/0));

  SetOrientation(views::LayoutOrientation::kHorizontal);
  SetMainAxisAlignment(views::LayoutAlignment::kCenter);
  SetInteriorMargin(kRefreshBannerInteriorMargin);
  SetID(mahi_constants::ViewId::kRefreshView);

  // We need to paint this view to a layer for animations.
  SetPaintToLayer();
  SetVisible(false);

  AddChildView(
      views::Builder<views::Label>()
          .SetText(l10n_util::GetStringFUTF16(
              IDS_ASH_MAHI_REFRESH_BANNER_LABEL_TEXT,
              manager ? manager->GetContentTitle() : base::EmptyString16()))
          .SetAutoColorReadabilityEnabled(false)
          .SetEnabledColorId(cros_tokens::kCrosSysSystemOnPrimaryContainer)
          .SetFontList(TypographyProvider::Get()->ResolveTypographyToken(
              TypographyToken::kCrosAnnotation2))
          .SetProperty(views::kMarginsKey, kTitleLabelMargin)
          .Build());
  auto* icon_button =
      AddChildView(IconButton::Builder()
                       .SetVectorIcon(&vector_icons::kReloadChromeRefreshIcon)
                       .SetType(IconButton::Type::kSmallProminentFloating)
                       .Build());
  icon_button->SetIconColor(cros_tokens::kCrosSysSystemOnPrimaryContainer);
}

RefreshBannerView::~RefreshBannerView() = default;

void RefreshBannerView::Show() {
  SetVisible(true);
  gfx::Transform transform;
  transform.Translate(
      gfx::Vector2d(0, mahi_constants::kRefreshBannerStackDepth));
  views::AnimationBuilder()
      .Once()
      .SetDuration(base::TimeDelta())
      .SetOpacity(this, 0)
      .SetTransform(this, transform)
      .At(base::Milliseconds(0))
      .SetDuration(kRefreshBannerAnimationDurationMs)
      .SetOpacity(this, 1)
      .SetTransform(this, gfx::Transform());
}

void RefreshBannerView::Hide() {
  views::AnimationBuilder()
      .OnEnded(base::BindOnce(
          [](base::WeakPtr<views::View> view) {
            if (view) {
              view->SetVisible(false);
            }
          },
          weak_ptr_factory_.GetWeakPtr()))
      .Once()
      .SetDuration(kRefreshBannerAnimationDurationMs)
      .SetOpacity(this, 0.0);
}

void RefreshBannerView::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  SetClipPath(GetClipPath(GetContentsBounds().size()));

  // Make sure the refresh banner is always shown on top.
  if (layer() && layer()->parent()) {
    layer()->parent()->StackAtTop(layer());
  }
}

BEGIN_METADATA(RefreshBannerView)
END_METADATA

}  // namespace ash
