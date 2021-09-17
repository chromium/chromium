// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/capture_mode/capture_mode_advanced_settings_view.h"

#include <memory>

#include "ash/capture_mode/capture_mode_bar_view.h"
#include "ash/capture_mode/capture_mode_constants.h"
#include "ash/capture_mode/capture_mode_menu_group.h"
#include "ash/capture_mode/capture_mode_toggle_button.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/background.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr gfx::Size kSettingsSize{256, 124};

constexpr gfx::RoundedCornersF kBorderRadius{10.f};

}  // namespace

CaptureModeAdvancedSettingsView::CaptureModeAdvancedSettingsView()
    : save_to_menu_group_(AddChildView(std::make_unique<CaptureModeMenuGroup>(
          kCaptureModeFolderIcon,
          IDS_ASH_SCREEN_CAPTURE_LABEL_SAVE_TO))) {
  save_to_menu_group_->AddOption(
      base::BindRepeating(&CaptureModeAdvancedSettingsView::HandleOptionClick,
                          base::Unretained(this)),
      IDS_ASH_SCREEN_CAPTURE_LABEL_SAVE_TO_DOWNLOADS, /*checked=*/true);
  save_to_menu_group_->AddMenuItem(
      base::BindRepeating(&CaptureModeAdvancedSettingsView::HandleMenuClick,
                          base::Unretained(this)),
      IDS_ASH_SCREEN_CAPTURE_LABEL_SAVE_TO_SELECT_FOLDER);

  SetPaintToLayer();
  SetBackground(
      views::CreateSolidBackground(AshColorProvider::Get()->GetBaseLayerColor(
          AshColorProvider::BaseLayerType::kTransparent80)));
  layer()->SetFillsBoundsOpaquely(false);
  layer()->SetRoundedCornerRadius(kBorderRadius);
  layer()->SetBackgroundBlur(ColorProvider::kBackgroundBlurSigma);
  layer()->SetBackdropFilterQuality(ColorProvider::kBackgroundBlurQuality);

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
}

CaptureModeAdvancedSettingsView::~CaptureModeAdvancedSettingsView() = default;

gfx::Rect CaptureModeAdvancedSettingsView::GetBounds(
    CaptureModeBarView* capture_mode_bar_view) {
  DCHECK(capture_mode_bar_view);

  return gfx::Rect(
      capture_mode_bar_view->settings_button()->GetBoundsInScreen().right() -
          kSettingsSize.width(),
      capture_mode_bar_view->GetBoundsInScreen().y() -
          capture_mode::kSpaceBetweenCaptureBarAndSettingsMenu -
          kSettingsSize.height(),
      kSettingsSize.width(), kSettingsSize.height());
}

void CaptureModeAdvancedSettingsView::HandleOptionClick() {}

void CaptureModeAdvancedSettingsView::HandleMenuClick() {}

BEGIN_METADATA(CaptureModeAdvancedSettingsView, views::View)
END_METADATA

}  // namespace ash