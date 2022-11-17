// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/unified_slider_view.h"

#include "ash/constants/quick_settings_catalogs.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "base/check_op.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/slider.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/widget/widget.h"

namespace ash {

using ContentLayerType = AshColorProvider::ContentLayerType;

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
                                     bool read_only)
    : button_(
          AddChildView(std::make_unique<IconButton>(std::move(callback),
                                                    IconButton::Type::kSmall,
                                                    &icon,
                                                    accessible_name_id,
                                                    /*is_togglable=*/true,
                                                    /*has_border=*/true))),
      slider_(
          AddChildView(CreateSlider(listener,
                                    read_only,
                                    QuickSettingsSlider::Style::kDefault))) {
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

  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(false);
}

std::unique_ptr<views::Slider> UnifiedSliderView::CreateSlider(
    UnifiedSliderListener* listener,
    bool read_only,
    QuickSettingsSlider::Style slider_style) {
  return read_only
             ? std::make_unique<ReadOnlySlider>(slider_style)
             : std::make_unique<QuickSettingsSlider>(listener, slider_style);
}

void UnifiedSliderView::SetSliderValue(float value, bool by_user) {
  // SetValue() calls |listener|, so we should ignore the call when the widget
  // is closed, because controllers are already deleted.
  // It should allow the case GetWidget() returning null, so that initial
  // position can be properly set by controllers before the view is attached to
  // a widget.
  if (GetWidget() && GetWidget()->IsClosed())
    return;

  slider_->SetValue(value);
  if (by_user)
    slider_->SetEnableAccessibilityEvents(true);
}

const char* UnifiedSliderView::GetClassName() const {
  return "UnifiedSliderView";
}

UnifiedSliderView::~UnifiedSliderView() = default;

void UnifiedSliderView::CreateToastLabel() {
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

}  // namespace ash
