// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_slider_view.h"

#include <memory>
#include <utility>

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/quick_settings_slider.h"
#include "base/check_op.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/base/models/image_model.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/events/keycodes/keyboard_codes_posix.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

namespace {

constexpr auto kQsSliderRowPadding = gfx::Insets::TLBR(0, 12, 4, 16);
constexpr auto kQsSliderBorder = gfx::Insets::TLBR(4, 4, 4, 16);

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
  CHECK_NE(GetCatalogName(), QsSliderCatalogName::kUnknown);
  quick_settings_metrics_util::RecordQsSliderToggle(
      GetCatalogName(), /*enable=*/target_toggle_state);
}

void UnifiedSliderListener::TrackValueChangeUMA(bool going_up) {
  CHECK_NE(GetCatalogName(), QsSliderCatalogName::kUnknown);
  quick_settings_metrics_util::RecordQsSliderValueChange(GetCatalogName(),
                                                         /*going_up=*/going_up);
}

UnifiedSliderView::UnifiedSliderView(views::Button::PressedCallback callback,
                                     UnifiedSliderListener* listener,
                                     const gfx::VectorIcon& icon,
                                     int accessible_name_id,
                                     bool is_togglable,
                                     bool read_only,
                                     QuickSettingsSlider::Style slider_style)
    : icon_(&icon), is_togglable_(is_togglable) {
  slider_ = AddChildView(CreateSlider(listener, read_only, slider_style));
  slider_->SetBorder(views::CreateEmptyBorder(kQsSliderBorder));
  // Sets `slider_` to have a `BoxLayout` to align the child view
  // `slider_button_` to the left.
  auto* slider_layout =
      slider_->SetLayoutManager(std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal));
  slider_layout->set_main_axis_alignment(
      views::BoxLayout::MainAxisAlignment::kStart);

  const bool is_default_style =
      (slider_style == QuickSettingsSlider::Style::kDefault ||
       slider_style == QuickSettingsSlider::Style::kDefaultMuted);
  slider_button_ = slider_->AddChildView(std::make_unique<IconButton>(
      std::move(callback),
      is_default_style ? IconButton::Type::kMediumFloating
                       : IconButton::Type::kLargeFloating,
      &icon, accessible_name_id,
      /*is_togglable=*/true,
      /*has_border=*/true));
  slider_button_->SetIconColor(cros_tokens::kCrosSysSystemOnPrimaryContainer);
  // The `slider_button_` should be focusable by the ChromeVox.
  slider_button_->SetFocusBehavior(FocusBehavior::ACCESSIBLE_ONLY);
  // `slider_button_` should disable event processing if it's not togglable.
  if (!is_togglable_) {
    slider_button_->SetCanProcessEventsWithinSubtree(/*can_process=*/false);
  }

  // Prevent an accessibility event while initiallizing this view.
  // Typically the first update of the slider value is conducted by the
  // caller function to reflect the current value.
  slider_->SetEnableAccessibilityEvents(false);
  slider_->GetViewAccessibility().SetName(
      l10n_util::GetStringUTF16(accessible_name_id),
      ax::mojom::NameFrom::kAttribute);

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kQsSliderRowPadding,
      kSliderChildrenViewSpacing));
  layout->SetFlexForView(slider_, /*flex=*/1);
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

void UnifiedSliderView::OnEvent(ui::Event* event) {
  // If `slider_button_` is not togglable, pressing the return key should not
  // trigger the clicking callback.
  if (!is_togglable_) {
    views::View::OnEvent(event);
    return;
  }

  if (!event->IsKeyEvent()) {
    views::View::OnEvent(event);
    return;
  }

  if (slider_ && !slider_->GetEnableAccessibilityEvents()) {
    slider_->SetEnableAccessibilityEvents(true);
  }

  auto* key_event = event->AsKeyEvent();
  auto key_code = key_event->key_code();

  // Only handles press event to avoid handling the event again when the key is
  // released.
  if (key_code == ui::VKEY_RETURN &&
      key_event->type() == ui::EventType::kKeyPressed) {
    slider_button_->NotifyClick(*event);
    return;
  }

  views::View::OnEvent(event);
}

BEGIN_METADATA(UnifiedSliderView)
END_METADATA

}  // namespace ash
