// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_mini_view_header_view.h"

#include "ash/style/ash_color_id.h"
#include "ash/wm/window_mini_view.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_constants.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/color/color_id.h"
#include "ui/color/color_provider.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/views/background.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout_view.h"
#include "ui/views/view_class_properties.h"
#include "ui/views/view_observer.h"

namespace ash {

namespace {

// The font delta of the window title.
constexpr int kLabelFontDelta = 2;

// Padding between header items.
constexpr int kHeaderPaddingDp = 8;

// The size in dp of the window icon shown on the alt-tab/overview window next
// to the title.
constexpr gfx::Size kIconSize = gfx::Size(24, 24);

constexpr gfx::Insets kHeaderInsets = gfx::Insets::TLBR(0, 10, 0, 10);

std::u16string GetWindowTitle(aura::Window* window) {
  aura::Window* transient_root = wm::GetTransientRoot(window);
  const std::u16string* overview_title =
      transient_root->GetProperty(chromeos::kWindowOverviewTitleKey);
  return (overview_title && !overview_title->empty())
             ? *overview_title
             : transient_root->GetTitle();
}

}  // namespace

WindowMiniViewHeaderView::~WindowMiniViewHeaderView() = default;

WindowMiniViewHeaderView::WindowMiniViewHeaderView(
    WindowMiniView* window_mini_view)
    : window_mini_view_(window_mini_view) {
  SetOrientation(views::BoxLayout::Orientation::kVertical);

  // This is to apply the rounded corners to child layers.
  SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  layer()->SetIsFastRoundedCorner(true);

  // Box layout should accomplish the following:
  // +------+-------+-------------------------------------------------+--------+
  // | icon | label |               leftover space                    | close  |
  // |      |       |                                                 | button |
  // +------+-------+-------------------------------------------------+--------+
  // * Note the close button may not be present in some corner cases, so it's
  //   created outside of `WindowMiniViewHeaderView`.
  // 1) The icon and close button get their preferred sizes.
  // 2) If the label's preferred size fits between the icon and close button,
  //    blank space is added between the label and close button until the close
  //    button is right aligned.
  // 3) If the label's preferred size doesn't fit between the icon and close
  //    button, it gets shrunk until it fits (leftover space above is zero).
  icon_label_view_ = AddChildView(std::make_unique<views::BoxLayoutView>());
  icon_label_view_->SetOrientation(views::BoxLayout::Orientation::kHorizontal);
  icon_label_view_->SetInsideBorderInsets(kHeaderInsets);
  icon_label_view_->SetBetweenChildSpacing(kHeaderPaddingDp);

  title_label_ = icon_label_view_->AddChildView(std::make_unique<views::Label>(
      GetWindowTitle(window_mini_view_->source_window())));
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetSubpixelRenderingEnabled(false);
  title_label_->SetFontList(gfx::FontList().Derive(
      kLabelFontDelta, gfx::Font::NORMAL, gfx::Font::Weight::MEDIUM));
  title_label_->SetEnabledColor(cros_tokens::kCrosSysPrimary);
  title_label_->SetPaintToLayer();
  title_label_->layer()->SetFillsBoundsOpaquely(false);
  icon_label_view_->SetFlexForView(title_label_, 1);

  RefreshHeaderViewRoundedCorners();

  separator_ = AddChildView(std::make_unique<views::View>());
  separator_->SetPaintToLayer(ui::LAYER_SOLID_COLOR);
  separator_->SetPreferredSize(gfx::Size(1, views::Separator::kThickness));

  SetFlexForView(icon_label_view_, 1);
}

void WindowMiniViewHeaderView::UpdateIconView(aura::Window* window) {
  aura::Window* transient_root = wm::GetTransientRoot(window);
  // Prefer kAppIconKey over kWindowIconKey as the app icon is typically
  // larger.
  gfx::ImageSkia* icon = transient_root->GetProperty(aura::client::kAppIconKey);
  if (!icon || icon->size().IsEmpty()) {
    icon = transient_root->GetProperty(aura::client::kWindowIconKey);
  }
  if (!icon) {
    return;
  }

  if (!icon_view_) {
    icon_view_ = icon_label_view_->AddChildViewAt(
        std::make_unique<views::ImageView>(), 0);
    icon_view_->SetPaintToLayer();
    icon_view_->layer()->SetFillsBoundsOpaquely(false);
  }

  icon_view_->SetImage(ui::ImageModel::FromImageSkia(
      gfx::ImageSkiaOperations::CreateResizedImage(
          *icon, skia::ImageOperations::RESIZE_BEST, kIconSize)));
}

void WindowMiniViewHeaderView::UpdateTitleLabel(aura::Window* window) {
  title_label_->SetText(GetWindowTitle(window));
}

void WindowMiniViewHeaderView::RefreshHeaderViewRoundedCorners() {
  const int default_corner_radius = kWindowMiniViewCornerRadius;
  const gfx::RoundedCornersF new_rounded_corners =
      custom_header_view_rounded_corners_.value_or(gfx::RoundedCornersF(
          default_corner_radius, default_corner_radius, 0, 0));
  if (current_header_view_rounded_corners_ &&
      *current_header_view_rounded_corners_ == new_rounded_corners) {
    return;
  }
  current_header_view_rounded_corners_ = new_rounded_corners;
  layer()->SetRoundedCornerRadius(new_rounded_corners);
}

void WindowMiniViewHeaderView::SetHeaderViewRoundedCornerRadius(
    gfx::RoundedCornersF& header_view_rounded_corners) {
  custom_header_view_rounded_corners_ = header_view_rounded_corners;
  RefreshHeaderViewRoundedCorners();
}

void WindowMiniViewHeaderView::ResetRoundedCorners() {
  custom_header_view_rounded_corners_.reset();
  RefreshHeaderViewRoundedCorners();
}

void WindowMiniViewHeaderView::OnThemeChanged() {
  View::OnThemeChanged();
  CHECK(GetColorProvider());
  layer()->SetColor(GetColorProvider()->GetColor(cros_tokens::kCrosSysHeader));
  separator_->layer()->SetColor(
      GetColorProvider()->GetColor(kColorAshWindowHeaderStrokeColor));
}

BEGIN_METADATA(WindowMiniViewHeaderView)
END_METADATA

}  // namespace ash
