// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_status_view.h"

#include "ash/public/cpp/network_icon_image_source.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/string16.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

using PhoneStatusModel = chromeos::phonehub::PhoneStatusModel;

namespace {

constexpr int kTitleContainerSpacing = 16;
constexpr int kStatusSpacing = 6;
constexpr gfx::Size kStatusIconSize(16, 16);
constexpr int kSeparatorHeight = 18;

int GetSignalStrengthAsInt(PhoneStatusModel::SignalStrength signal_strength) {
  switch (signal_strength) {
    case PhoneStatusModel::SignalStrength::kZeroBars:
      return 0;
    case PhoneStatusModel::SignalStrength::kOneBar:
      return 1;
    case PhoneStatusModel::SignalStrength::kTwoBars:
      return 2;
    case PhoneStatusModel::SignalStrength::kThreeBars:
      return 3;
    case PhoneStatusModel::SignalStrength::kFourBars:
      return 4;
  }
}

}  // namespace

PhoneStatusView::PhoneStatusView(chromeos::phonehub::PhoneModel* phone_model)
    : TriView(kTitleContainerSpacing),
      phone_model_(phone_model),
      phone_name_label_(new views::Label),
      signal_icon_(new views::ImageView),
      mobile_provider_label_(new views::Label),
      battery_icon_(new views::ImageView),
      battery_label_(new views::Label) {
  SetID(PhoneHubViewID::kPhoneStatusView);

  ConfigureTriViewContainer(TriView::Container::START);
  ConfigureTriViewContainer(TriView::Container::CENTER);
  ConfigureTriViewContainer(TriView::Container::END);

  phone_model_->AddObserver(this);

  phone_name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SUB_HEADER,
                           true /* use_unified_theme */);
  style.SetupLabel(phone_name_label_);
  AddView(TriView::Container::CENTER, phone_name_label_);

  AddView(TriView::Container::END, signal_icon_);

  mobile_provider_label_->SetAutoColorReadabilityEnabled(false);
  mobile_provider_label_->SetSubpixelRenderingEnabled(false);
  mobile_provider_label_->SetEnabledColor(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
  AddView(TriView::Container::END, mobile_provider_label_);

  AddView(TriView::Container::END, battery_icon_);

  battery_label_->SetAutoColorReadabilityEnabled(false);
  battery_label_->SetSubpixelRenderingEnabled(false);
  battery_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));
  AddView(TriView::Container::END, battery_label_);

  auto* separator = new views::Separator();
  separator->SetColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kSeparatorColor));
  separator->SetPreferredHeight(kSeparatorHeight);
  AddView(TriView::Container::END, separator);

  settings_button_ = new TopShortcutButton(this, kSystemMenuSettingsIcon,
                                           IDS_ASH_STATUS_TRAY_SETTINGS);
  AddView(TriView::Container::END, settings_button_);

  Update();
}

PhoneStatusView::~PhoneStatusView() {
  phone_model_->RemoveObserver(this);
}

void PhoneStatusView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  // TODO(leandre): implement open settings/other buttons.
}

void PhoneStatusView::OnModelChanged() {
  Update();
}

void PhoneStatusView::Update() {
  phone_name_label_->SetText(
      phone_model_->phone_name().value_or(base::string16()));

  // Clear the phone status if the status model returns null when the phone is
  // disconnected.
  if (!phone_model_->phone_status_model()) {
    ClearExistingStatus();
    return;
  }

  UpdateMobileStatus();
  UpdateBatteryStatus();
}

void PhoneStatusView::UpdateMobileStatus() {
  const PhoneStatusModel& phone_status =
      phone_model_->phone_status_model().value();

  const SkColor primary_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);

  gfx::ImageSkia signal_image;
  switch (phone_status.mobile_status()) {
    case PhoneStatusModel::MobileStatus::kNoSim:
      signal_image = CreateVectorIcon(kPhoneHubMobileNoSimIcon, primary_color);
      break;
    case PhoneStatusModel::MobileStatus::kSimButNoReception:
      signal_image =
          CreateVectorIcon(kPhoneHubMobileNoConnectionIcon, primary_color);
      break;
    case PhoneStatusModel::MobileStatus::kSimWithReception:
      const PhoneStatusModel::MobileConnectionMetadata& metadata =
          phone_status.mobile_connection_metadata().value();
      int signal_strength = GetSignalStrengthAsInt(metadata.signal_strength);
      signal_image = gfx::CanvasImageSource::MakeImageSkia<
          network_icon::SignalStrengthImageSource>(
          network_icon::ImageType::BARS, primary_color, kStatusIconSize,
          signal_strength);
      mobile_provider_label_->SetText(metadata.mobile_provider);
      break;
  }

  signal_icon_->SetImage(signal_image);
  mobile_provider_label_->SetVisible(
      phone_status.mobile_status() ==
      PhoneStatusModel::MobileStatus::kSimWithReception);
}

void PhoneStatusView::UpdateBatteryStatus() {
  const PhoneStatusModel& phone_status =
      phone_model_->phone_status_model().value();

  const PowerStatus::BatteryImageInfo& info = CalculateBatteryInfo();

  const SkColor icon_bg_color = color_utils::GetResultingPaintColor(
      ShelfConfig::Get()->GetShelfControlButtonColor(),
      AshColorProvider::Get()->GetBackgroundColor());
  const SkColor icon_fg_color = AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kIconColorPrimary);

  battery_icon_->SetImage(PowerStatus::GetBatteryImage(
      info, kUnifiedTrayIconSize, icon_bg_color, icon_fg_color));
  battery_label_->SetText(
      base::FormatPercent(phone_status.battery_percentage()));
}

PowerStatus::BatteryImageInfo PhoneStatusView::CalculateBatteryInfo() {
  PowerStatus::BatteryImageInfo info;

  const PhoneStatusModel& phone_status =
      phone_model_->phone_status_model().value();

  info.charge_percent = phone_status.battery_percentage();

  switch (phone_status.charging_state()) {
    case PhoneStatusModel::ChargingState::kNotCharging:
      info.alert_if_low = true;
      if (info.charge_percent < PowerStatus::kCriticalBatteryChargePercentage) {
        info.icon_badge = &kUnifiedMenuBatteryAlertIcon;
        info.badge_outline = &kUnifiedMenuBatteryAlertOutlineIcon;
      }
      break;
    case PhoneStatusModel::ChargingState::kChargingAc:
      info.icon_badge = &kUnifiedMenuBatteryBoltIcon;
      info.badge_outline = &kUnifiedMenuBatteryBoltOutlineIcon;
      break;
    case PhoneStatusModel::ChargingState::kChargingUsb:
      info.icon_badge = &kUnifiedMenuBatteryUnreliableIcon;
      info.badge_outline = &kUnifiedMenuBatteryUnreliableOutlineIcon;
      break;
  }

  return info;
}

void PhoneStatusView::ClearExistingStatus() {
  // Clear mobile status.
  signal_icon_->SetImage(gfx::ImageSkia());
  mobile_provider_label_->SetText(base::string16());

  // Clear battery status.
  battery_icon_->SetImage(gfx::ImageSkia());
  battery_label_->SetText(base::string16());
}

void PhoneStatusView::ConfigureTriViewContainer(TriView::Container container) {
  std::unique_ptr<views::BoxLayout> layout;

  switch (container) {
    case TriView::Container::START:
      FALLTHROUGH;
    case TriView::Container::END:
      layout = std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kStatusSpacing);
      layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);
      break;
    case TriView::Container::CENTER:
      SetFlexForContainer(TriView::Container::CENTER, 1.f);

      layout = std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical);
      layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);
      break;
  }

  SetContainerLayout(container, std::move(layout));
  SetMinSize(container, gfx::Size(0, kUnifiedDetailedViewTitleRowHeight));
}

}  // namespace ash
