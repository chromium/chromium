// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/buttons.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/quick_settings_catalogs.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/power/power_status.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/unified/quick_settings_metrics_util.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "base/functional/bind.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/compositor/layer.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Helper function for getting ContentLayerColor.
inline SkColor GetContentLayerColor(AshColorProvider::ContentLayerType type) {
  return AshColorProvider::Get()->GetContentLayerColor(type);
}

// Helper function for configuring label in BatteryInfoView.
void ConfigureLabel(views::Label* label, SkColor color) {
  label->SetAutoColorReadabilityEnabled(false);
  label->SetSubpixelRenderingEnabled(false);
  label->SetEnabledColor(color);
  label->GetViewAccessibility().OverrideIsIgnored(true);
}

}  // namespace

BatteryInfoViewBase::BatteryInfoViewBase(
    UnifiedSystemTrayController* controller)
    : Button(base::BindRepeating(
          [](UnifiedSystemTrayController* controller) {
            quick_settings_metrics_util::RecordQsButtonActivated(
                QsButtonCatalogName::kBatteryButton);
            controller->HandleOpenPowerSettingsAction();
          },
          controller)) {
  PowerStatus::Get()->AddObserver(this);
}

BatteryInfoViewBase::~BatteryInfoViewBase() {
  PowerStatus::Get()->RemoveObserver(this);
}

void BatteryInfoViewBase::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ax::mojom::Role::kLabelText;
  node_data->SetName(PowerStatus::Get()->GetAccessibleNameString(true));
}

void BatteryInfoViewBase::ChildPreferredSizeChanged(views::View* child) {
  PreferredSizeChanged();
}

void BatteryInfoViewBase::ChildVisibilityChanged(views::View* child) {
  PreferredSizeChanged();
}

// PowerStatus::Observer:
void BatteryInfoViewBase::OnPowerStatusChanged() {
  Update();
}

BEGIN_METADATA(BatteryInfoViewBase, views::Button)
END_METADATA

BatteryLabelView::BatteryLabelView(UnifiedSystemTrayController* controller,
                                   bool use_smart_charging_ui)
    : BatteryInfoViewBase(controller),
      use_smart_charging_ui_(use_smart_charging_ui) {
  SetID(VIEW_ID_QS_BATTERY_BUTTON);
  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal));

  percentage_ = AddChildView(std::make_unique<views::Label>());
  auto seperator = std::make_unique<views::Label>();
  seperator->SetText(
      l10n_util::GetStringUTF16(IDS_ASH_STATUS_TRAY_BATTERY_STATUS_SEPARATOR));
  separator_view_ = AddChildView(std::move(seperator));
  status_ = AddChildView(std::make_unique<views::Label>());
  Update();
}

BatteryLabelView::~BatteryLabelView() = default;

void BatteryLabelView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const auto color = GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorSecondary);
  ConfigureLabel(percentage_, color);
  ConfigureLabel(separator_view_, color);
  ConfigureLabel(status_, color);
}

void BatteryLabelView::Update() {
  std::u16string percentage_text;
  std::u16string status_text;
  std::tie(percentage_text, status_text) =
      PowerStatus::Get()->GetStatusStrings();

  percentage_->SetText(percentage_text);
  status_->SetText(status_text);

  percentage_->SetVisible(!percentage_text.empty() && !use_smart_charging_ui_);
  separator_view_->SetVisible(!percentage_text.empty() &&
                              !use_smart_charging_ui_ && !status_text.empty());
  status_->SetVisible(!status_text.empty());

  percentage_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
  status_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);
}

BEGIN_METADATA(BatteryLabelView, BatteryInfoViewBase)
END_METADATA

BatteryIconView::BatteryIconView(UnifiedSystemTrayController* controller)
    : BatteryInfoViewBase(controller) {
  SetID(VIEW_ID_QS_BATTERY_BUTTON);
  auto layout = std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal);
  layout->set_inside_border_insets(kUnifiedSystemInfoBatteryIconPadding);
  SetLayoutManager(std::move(layout));

  battery_image_ = AddChildView(std::make_unique<views::ImageView>());
  if (features::IsDarkLightModeEnabled()) {
    // The battery icon requires its own layer to properly render the masked
    // outline of the badge within the battery icon.
    battery_image_->SetPaintToLayer();
    battery_image_->layer()->SetFillsBoundsOpaquely(false);
  }
  ConfigureIcon();

  percentage_ = AddChildView(std::make_unique<views::Label>());

  SetBackground(views::CreateRoundedRectBackground(
      GetContentLayerColor(AshColorProvider::ContentLayerType::
                               kBatterySystemInfoBackgroundColor),
      GetPreferredSize().height() / 2));

  Update();
}

BatteryIconView::~BatteryIconView() = default;

void BatteryIconView::OnThemeChanged() {
  views::View::OnThemeChanged();
  const auto color = GetContentLayerColor(
      AshColorProvider::ContentLayerType::kButtonLabelColorPrimary);
  ConfigureLabel(percentage_, color);
  ConfigureIcon();
}

void BatteryIconView::Update() {
  const std::u16string percentage_text =
      PowerStatus::Get()->GetStatusStrings().first;

  percentage_->SetText(percentage_text);
  percentage_->SetVisible(!percentage_text.empty());
  percentage_->NotifyAccessibilityEvent(ax::mojom::Event::kTextChanged, true);

  ConfigureIcon();
}

void BatteryIconView::ConfigureIcon() {
  const SkColor battery_icon_color = GetContentLayerColor(
      AshColorProvider::ContentLayerType::kBatterySystemInfoIconColor);

  const SkColor badge_color = GetContentLayerColor(
      AshColorProvider::ContentLayerType::kBatterySystemInfoBackgroundColor);

  PowerStatus::BatteryImageInfo info =
      PowerStatus::Get()->GetBatteryImageInfo();
  info.alert_if_low = false;
  battery_image_->SetImage(PowerStatus::GetBatteryImage(
      info, kUnifiedTrayBatteryIconSize, battery_icon_color, battery_icon_color,
      badge_color));
}

BEGIN_METADATA(BatteryIconView, BatteryInfoViewBase)
END_METADATA

}  // namespace ash
