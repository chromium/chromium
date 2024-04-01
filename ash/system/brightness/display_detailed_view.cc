// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/brightness/display_detailed_view.h"

#include <memory>

#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/brightness/unified_brightness_slider_controller.h"
#include "ash/system/dark_mode/dark_mode_feature_pod_controller.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/night_light/night_light_feature_pod_controller.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/feature_tile.h"
#include "ash/system/unified/unified_system_tray.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/views/border.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/layout/flex_layout.h"
#include "ui/views/layout/flex_layout_types.h"
#include "ui/views/layout/flex_layout_view.h"
#include "ui/views/view.h"
#include "ui/views/view_class_properties.h"

namespace ash {

namespace {

constexpr auto kScrollViewMargin = gfx::Insets::TLBR(0, 12, 16, 12);
constexpr auto kSliderBorder = gfx::Insets(4);
constexpr auto kSliderPadding = gfx::Insets::TLBR(0, 0, 4, 0);
constexpr auto kTileContainerMargins = gfx::Insets::TLBR(4, 4, 12, 4);

}  // namespace

DisplayDetailedView::DisplayDetailedView(
    DetailedViewDelegate* delegate,
    UnifiedSystemTrayController* tray_controller)
    : TrayDetailedView(delegate),
      unified_system_tray_controller_(tray_controller) {
  CreateScrollableList();
  CreateTitleRow(IDS_ASH_STATUS_TRAY_DISPLAY);
  // Sets the margin for `ScrollView` to leave some space for the focus ring.
  scroller()->SetProperty(views::kMarginsKey, kScrollViewMargin);

  auto night_light_controller =
      std::make_unique<NightLightFeaturePodController>(
          unified_system_tray_controller_);
  auto dark_mode_controller = std::make_unique<DarkModeFeaturePodController>(
      unified_system_tray_controller_);

  auto tile_container =
      views::Builder<views::FlexLayoutView>()
          .SetID(VIEW_ID_QS_DISPLAY_TILE_CONTAINER)
          .SetPreferredSize(
              gfx::Size(GetPreferredSize().width(),
                        kFeatureTileHeight + kTileContainerMargins.height()))
          .CustomConfigure(base::BindOnce([](views::FlexLayoutView* layout) {
            layout->SetDefault(views::kMarginsKey, kTileContainerMargins);
          }))
          .Build();

  tile_container->AddChildView(night_light_controller->CreateTile());
  tile_container->AddChildView(dark_mode_controller->CreateTile());

  bool has_visible_tiles = false;

  // Set `PreferredSize` to (1,1) so the `FlexLayout` allocates the same width
  // for the child tiles based on their equal weights.
  for (auto tile : tile_container->children()) {
    tile->SetPreferredSize(gfx::Size(1, 1));
    if (tile->GetVisible()) {
      has_visible_tiles = true;
    }
  }

  if (!has_visible_tiles) {
    tile_container->SetVisible(false);
  }

  // Transfer ownership so the controllers won't die while the page is open.
  feature_tile_controllers_.push_back(std::move(night_light_controller));
  feature_tile_controllers_.push_back(std::move(dark_mode_controller));

  scroll_content()->AddChildView(std::move(tile_container));

  brightness_slider_controller_ =
      std::make_unique<UnifiedBrightnessSliderController>(
          Shell::GetPrimaryRootWindowController()
              ->GetStatusAreaWidget()
              ->unified_system_tray()
              ->model(),
          views::Button::PressedCallback());
  auto unified_brightness_view =
      brightness_slider_controller_->CreateBrightnessSlider();
  // Sets the ID for testing.
  unified_brightness_view->SetID(VIEW_ID_QS_DISPLAY_BRIGHTNESS_SLIDER);
  unified_brightness_view->slider()->SetBorder(
      views::CreateEmptyBorder(kSliderBorder));
  auto* slider_layout = unified_brightness_view->SetLayoutManager(
      std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kSliderPadding,
          /*between_child_spacing=*/0));
  slider_layout->SetFlexForView(unified_brightness_view->slider(),
                                /*flex=*/1);
  scroll_content()->AddChildView(std::move(unified_brightness_view));
  // Sets the ID for testing.
  scroll_content()->SetID(VIEW_ID_QS_DISPLAY_SCROLL_CONTENT);
}

DisplayDetailedView::~DisplayDetailedView() = default;

views::View* DisplayDetailedView::GetScrollContentForTest() {
  // Provides access to the protected scroll_content() in the base class.
  return scroll_content();
}

void DisplayDetailedView::CreateExtraTitleRowButtons() {
  CHECK(!settings_button_);

  tri_view()->SetContainerVisible(TriView::Container::END, /*visible=*/true);

  settings_button_ = CreateSettingsButton(
      base::BindRepeating(&DisplayDetailedView::OnSettingsClicked,
                          weak_factory_.GetWeakPtr()),
      IDS_ASH_STATUS_TRAY_NIGHT_LIGHT_SETTINGS_TOOLTIP);
  settings_button_->SetState(TrayPopupUtils::CanOpenWebUISettings()
                                 ? views::Button::STATE_NORMAL
                                 : views::Button::STATE_DISABLED);
  tri_view()->AddView(TriView::Container::END, settings_button_);
}

void DisplayDetailedView::OnSettingsClicked() {
  if (TrayPopupUtils::CanOpenWebUISettings()) {
    CloseBubble();
    Shell::Get()->system_tray_model()->client()->ShowDisplaySettings();
  }
}

BEGIN_METADATA(DisplayDetailedView)
END_METADATA

}  // namespace ash
