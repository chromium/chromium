// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_slider_view.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "base/check_op.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

using ContentLayerType = AshColorProvider::ContentLayerType;

namespace {

constexpr auto kQsSliderRowPadding = gfx::Insets::TLBR(0, 12, 4, 16);
constexpr int kQsSliderViewSpacing = 8;
constexpr auto kQsSliderIconInsets = gfx::Insets::VH(0, 10);
constexpr auto kQsSliderBorder = gfx::Insets::TLBR(0, 4, 0, 16);

std::unique_ptr<views::Slider> CreateSlider(
    UnifiedSliderListener* listener,
    bool read_only,
    QuickSettingsSlider::Style slider_style) {
  return read_only
             ? std::make_unique<ReadOnlySlider>(slider_style)
             : std::make_unique<QuickSettingsSlider>(listener, slider_style);
}

}  // namespace

void UnifiedSliderListener::TrackToggleUMA(bool target_toggle_state) {
  DCHECK_NE(GetCatalogName(), QsSliderCatalogName::kUnknown);
  quick_settings_metrics_util::RecordQsSliderToggle(
      GetCatalogName(), /*enable=*/target_toggle_state);
}

void UnifiedSliderListener::TrackValueChangeUMA(bool going_up) {
  DCHECK_NE(GetCatalogName(), QsSliderCatalogName::kUnknown);
  quick_settings_metrics_util::RecordQsSliderValueChange(GetCatalogName(),
                                                         /*going_up=*/going_up);
}

UnifiedSliderView::UnifiedSliderView(views::Button::PressedCallback callback,
                                     UnifiedSliderListener* listener,
                                     const gfx::VectorIcon& icon,
                                     int accessible_name_id,
                                     bool read_only,
                                     QuickSettingsSlider::Style slider_style)
    : icon_(&icon),
      accessible_name_id_(accessible_name_id),
      callback_(callback) {
  if (!features::IsQsRevampEnabled()) {
    button_ = AddChildView(std::make_unique<IconButton>(
        std::move(callback), IconButton::Type::kMedium, &icon,
        accessible_name_id,
        /*is_togglable=*/true,
        /*has_border=*/true));

    slider_ = AddChildView(CreateSlider(listener, read_only, slider_style));

    auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
        views::BoxLayout::Orientation::kHorizontal, kUnifiedSliderRowPadding,
        kUnifiedSliderViewSpacing));

    // Prevent an accessibility event while initiallizing this view. Typically
    // the first update of the slider value is conducted by the caller function
    // to reflect the current value.
    slider_->SetEnableAccessibilityEvents(false);

    slider_->GetViewAccessibility().OverrideName(
        l10n_util::GetStringUTF16(accessible_name_id));
    slider_->SetBorder(views::CreateEmptyBorder(kUnifiedSliderPadding));
    slider_->SetPreferredSize(gfx::Size(0, kTrayItemSize));
    layout->SetFlexForView(slider_, 1);
    layout->set_cross_axis_alignment(
        views::BoxLayout::CrossAxisAlignment::kCenter);

    // Adds a layer to set it non-opaque. Otherwise the previous draw of thumb
    // will stay.
    SetPaintToLayer();
    layer()->SetFillsBoundsOpaquely(false);
    return;
  }

  auto container = std::make_unique<views::View>();
  slider_ =
      container->AddChildView(CreateSlider(listener, read_only, slider_style));
  // Uses `icon_container` to hold `slider_icon_` and makes it left align.
  auto icon_container = std::make_unique<views::View>();
  icon_container->SetCanProcessEventsWithinSubtree(false);

  slider_icon_ =
      icon_container->AddChildView(std::make_unique<views::ImageView>());
  slider_icon_->SetImage(ui::ImageModel::FromVectorIcon(
      icon, cros_tokens::kCrosSysSystemOnPrimaryContainer, kQsSliderIconSize));
  // Sets up the `slider_icon_` for RTL since `ImageView` doesn't handle it.
  slider_icon_->SetFlipCanvasOnPaintForRTLUI(true);

  auto* icon_container_layout =
      icon_container->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, kQsSliderIconInsets,
          /*between_child_spacing=*/0));
  icon_container_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  container->AddChildView(std::move(icon_container));
  container->SetLayoutManager(std::make_unique<views::FillLayout>());

  // Prevent an accessibility event while initiallizing this view.
  // Typically the first update of the slider value is conducted by the
  // caller function to reflect the current value.
  slider_->SetEnableAccessibilityEvents(false);

  slider_->GetViewAccessibility().OverrideName(
      l10n_util::GetStringUTF16(accessible_name_id));
  slider_->SetBorder(views::CreateEmptyBorder(kQsSliderBorder));

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kQsSliderRowPadding,
      kQsSliderViewSpacing));
  container_ = AddChildView(std::move(container));
  layout->SetFlexForView(container_, /*flex=*/1);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kCenter);

  // Adds a layer to set it non-opaque. Otherwise the full part of the slider
  // will be drawn on top of the previous draw.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

void UnifiedSliderView::SetSliderValue(float value, bool by_user) {
  // SetValue() calls |listener|, so we should ignore the call when the widget
  // is closed, because controllers are already deleted.
  // It should allow the case GetWidget() returning null, so that initial
  // position can be properly set by controllers before the view is attached to
  // a widget.
  if (GetWidget() && GetWidget()->IsClosed()) {
    return;
  }

  slider_->SetValue(value);
  if (by_user) {
    slider_->SetEnableAccessibilityEvents(true);
  }
}

UnifiedSliderView::~UnifiedSliderView() = default;

void UnifiedSliderView::CreateToastLabel() {
  if (features::IsQsRevampEnabled()) {
    button_ = AddChildView(std::make_unique<IconButton>(
        std::move(callback_), IconButton::Type::kMedium, icon_,
        accessible_name_id_,
        /*is_togglable=*/true,
        /*has_border=*/true));
    container_->SetVisible(false);
  }
  toast_label_ = AddChildView(std::make_unique<views::Label>());
  TrayPopupUtils::SetLabelFontList(toast_label_,
                                   TrayPopupUtils::FontStyle::kPodMenuHeader);
}

void UnifiedSliderView::OnThemeChanged() {
  views::View::OnThemeChanged();
  if (toast_label_) {
    toast_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
        AshColorProvider::ContentLayerType::kTextColorPrimary));
  }
}

BEGIN_METADATA(UnifiedSliderView, views::View)
END_METADATA

}  // namespace ash
