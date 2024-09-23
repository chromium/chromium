// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_status_view.h"

#include <string>

#include "ash/public/cpp/network_icon_image_source.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/root_window_controller.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_id.h"
#include "ash/style/ash_color_provider.h"
#include "ash/style/icon_button.h"
#include "ash/style/typography.h"
#include "ash/system/phonehub/phone_hub_tray.h"
#include "ash/system/phonehub/phone_hub_view_ids.h"
#include "ash/system/status_area_widget.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_utils.h"
#include "base/functional/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/string_number_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/color/color_id.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/gfx/text_elider.h"
#include "ui/views/accessibility/view_accessibility.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/separator.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

using PhoneStatusModel = phonehub::PhoneStatusModel;

// Appearance in Dip.
constexpr int kTitleContainerSpacing = 16;
constexpr int kStatusSpacing = 4;
constexpr gfx::Size kStatusIconSize(kUnifiedTrayIconSize, kUnifiedTrayIconSize);
constexpr gfx::Size kSignalIconSize(15, 15);
constexpr int kSeparatorHeight = 18;
constexpr int kPhoneNameLabelWidthMax = 160;
constexpr auto kBorderInsets = gfx::Insets::VH(0, 16);
constexpr auto kBatteryLabelBorderInsets = gfx::Insets::TLBR(0, 0, 0, 4);

// Multiplied by the int returned by GetSignalStrengthAsInt() to obtain a
// percentage for the signal strength displayed by the tooltip when hovering
// over the signal strength icon, and verbalized by ChromeVox.
constexpr int kSignalStrengthToPercentageMultiplier = 25;

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

bool IsBatterySaverModeOn(const PhoneStatusModel& phone_status) {
  return phone_status.battery_saver_state() ==
         PhoneStatusModel::BatterySaverState::kOn;
}

}  // namespace

PhoneStatusView::PhoneStatusView(phonehub::PhoneModel* phone_model,
                                 Delegate* delegate)
    : TriView(kTitleContainerSpacing),
      phone_model_(phone_model),
      phone_name_label_(new views::Label),
      signal_icon_(new views::ImageView),
      battery_icon_(new views::ImageView),
      battery_label_(new views::Label) {
  DCHECK(delegate);
  SetID(PhoneHubViewID::kPhoneStatusView);

  SetBorder(views::CreateEmptyBorder(kBorderInsets));

  // Phone name is placed at START container, Settings icon is
  // placed at END container, other phone states, i.e. battery level,
  // and Separator are placed at CENTER container.
  ConfigureTriViewContainer(TriView::Container::START);
  ConfigureTriViewContainer(TriView::Container::CENTER);
  ConfigureTriViewContainer(TriView::Container::END);

  phone_model_->AddObserver(this);

  phone_name_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  // TODO(b/322067753): Replace usage of |AshColorProvider| with |cros_tokens|.
  phone_name_label_->SetEnabledColor(
      AshColorProvider::Get()->GetContentLayerColor(
          AshColorProvider::ContentLayerType::kTextColorPrimary));
  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosHeadline1,
                                        *phone_name_label_);

  phone_name_label_->SetElideBehavior(gfx::ElideBehavior::ELIDE_TAIL);
  AddView(TriView::Container::START, phone_name_label_);

  AddView(TriView::Container::CENTER, signal_icon_);

  // The battery icon requires its own layer to properly render the masked
  // outline of the badge within the battery icon.
  battery_icon_->SetPaintToLayer();
  battery_icon_->layer()->SetFillsBoundsOpaquely(false);
  AddView(TriView::Container::CENTER, battery_icon_);

  battery_label_->SetAutoColorReadabilityEnabled(false);
  battery_label_->SetSubpixelRenderingEnabled(false);

  // TODO(b/322067753): Replace usage of |AshColorProvider| with |cros_tokens|.
  battery_label_->SetEnabledColor(AshColorProvider::Get()->GetContentLayerColor(
      AshColorProvider::ContentLayerType::kTextColorPrimary));

  TypographyProvider::Get()->StyleLabel(ash::TypographyToken::kCrosButton2,
                                        *battery_label_);

  battery_label_->SetBorder(
      views::CreateEmptyBorder(kBatteryLabelBorderInsets));
  AddView(TriView::Container::CENTER, battery_label_);

  separator_ = new views::Separator();
  separator_->SetColorId(ui::kColorAshSystemUIMenuSeparator);
  separator_->SetPreferredLength(kSeparatorHeight);
  AddView(TriView::Container::CENTER, separator_);

  settings_button_ = new IconButton(
      base::BindRepeating(&Delegate::OpenConnectedDevicesSettings,
                          base::Unretained(delegate)),
      IconButton::Type::kMedium, &kSystemMenuSettingsIcon,
      IDS_ASH_PHONE_HUB_CONNECTED_DEVICE_SETTINGS_LABEL);
  AddView(TriView::Container::END, settings_button_);

  separator_->SetVisible(delegate->CanOpenConnectedDeviceSettings());
  settings_button_->SetVisible(delegate->CanOpenConnectedDeviceSettings());
}

PhoneStatusView::~PhoneStatusView() {
  phone_model_->RemoveObserver(this);
}

void PhoneStatusView::OnThemeChanged() {
  TriView::OnThemeChanged();
  Update();
}

void PhoneStatusView::OnModelChanged() {
  Update();
}

void PhoneStatusView::Update() {
  // Set phone name text and elide it if needed.
  phone_name_label_->SetText(
      gfx::ElideText(phone_model_->phone_name().value_or(std::u16string()),
                     phone_name_label_->font_list(), kPhoneNameLabelWidthMax,
                     gfx::ELIDE_TAIL));

  // Clear the phone status if the status model returns null when the phone is
  // disconnected.
  if (!phone_model_->phone_status_model()) {
    ClearExistingStatus();
    // Hide separator if there is no preceding content.
    separator_->SetVisible(false);
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
  std::u16string tooltip_text;
  switch (phone_status.mobile_status()) {
    case PhoneStatusModel::MobileStatus::kNoSim:
      signal_image = CreateVectorIcon(kPhoneHubMobileNoSimIcon, primary_color);
      tooltip_text =
          l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_MOBILE_STATUS_NO_SIM);
      signal_icon_->SetImageSize(kStatusIconSize);
      break;
    case PhoneStatusModel::MobileStatus::kSimButNoReception:
      signal_image =
          CreateVectorIcon(kPhoneHubMobileNoConnectionIcon, primary_color);
      tooltip_text =
          l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_MOBILE_STATUS_NO_NETWORK);
      signal_icon_->SetImageSize(kStatusIconSize);
      break;
    case PhoneStatusModel::MobileStatus::kSimWithReception:
      const PhoneStatusModel::MobileConnectionMetadata& metadata =
          phone_status.mobile_connection_metadata().value();
      int signal_strength = GetSignalStrengthAsInt(metadata.signal_strength);
      signal_image = gfx::CanvasImageSource::MakeImageSkia<
          network_icon::SignalStrengthImageSource>(
          network_icon::ImageType::BARS, primary_color, kSignalIconSize,
          signal_strength);
      signal_icon_->SetImageSize(kSignalIconSize);
      tooltip_text = l10n_util::GetStringFUTF16(
          IDS_ASH_PHONE_HUB_MOBILE_STATUS_NETWORK_NAME_AND_STRENGTH,
          metadata.mobile_provider,
          base::NumberToString16(signal_strength *
                                 kSignalStrengthToPercentageMultiplier));
      break;
  }

  signal_icon_->SetImage(signal_image);
  signal_icon_->SetTooltipText(tooltip_text);
}

void PhoneStatusView::UpdateBatteryStatus() {
  const PhoneStatusModel& phone_status =
      phone_model_->phone_status_model().value();

  const SkColor icon_fg_color = AshColorProvider::Get()->GetContentLayerColor(
      IsBatterySaverModeOn(phone_status)
          ? AshColorProvider::ContentLayerType::kIconColorWarning
          : AshColorProvider::ContentLayerType::kIconColorPrimary);

  battery_icon_->SetImage(PowerStatus::GetBatteryImage(
      CalculateBatteryInfo(icon_fg_color), kUnifiedTrayBatteryIconSize,
      battery_icon_->GetColorProvider()));
  SetBatteryTooltipText();
  battery_label_->SetText(
      base::FormatPercent(phone_status.battery_percentage()));
  battery_label_->GetViewAccessibility().SetName(l10n_util::GetStringFUTF16(
      IDS_ASH_PHONE_HUB_BATTERY_PERCENTAGE_ACCESSIBLE_TEXT,
      base::NumberToString16(phone_status.battery_percentage())));
}

PowerStatus::BatteryImageInfo PhoneStatusView::CalculateBatteryInfo(
    const SkColor icon_fg_color) {
  const PhoneStatusModel& phone_status =
      phone_model_->phone_status_model().value();
  PowerStatus::BatteryImageInfo info(icon_fg_color);
  info.charge_percent = phone_status.battery_percentage();

  if (IsBatterySaverModeOn(phone_status)) {
    info.icon_badge = &kPhoneHubBatterySaverIcon;
    info.badge_outline = &kPhoneHubBatterySaverOutlineMaskIcon;
    return info;
  }

  switch (phone_status.charging_state()) {
    case PhoneStatusModel::ChargingState::kNotCharging:
      info.alert_if_low = true;
      if (info.charge_percent < PowerStatus::kCriticalBatteryChargePercentage) {
        info.icon_badge = &kUnifiedMenuBatteryAlertIcon;
        info.badge_outline = &kUnifiedMenuBatteryAlertOutlineMaskIcon;
      }
      break;
    case PhoneStatusModel::ChargingState::kChargingAc:
      info.icon_badge = &kUnifiedMenuBatteryBoltIcon;
      info.badge_outline = &kUnifiedMenuBatteryBoltOutlineMaskIcon;
      break;
    case PhoneStatusModel::ChargingState::kChargingUsb:
      info.icon_badge = &kUnifiedMenuBatteryUnreliableIcon;
      info.badge_outline = &kUnifiedMenuBatteryUnreliableOutlineMaskIcon;
      break;
  }

  return info;
}

void PhoneStatusView::SetBatteryTooltipText() {
  const PhoneStatusModel& phone_status =
      phone_model_->phone_status_model().value();

  int charging_tooltip_id;
  switch (phone_status.charging_state()) {
    case PhoneStatusModel::ChargingState::kNotCharging:
      charging_tooltip_id = IDS_ASH_PHONE_HUB_BATTERY_STATUS_NOT_CHARGING;
      break;
    case PhoneStatusModel::ChargingState::kChargingAc:
      charging_tooltip_id = IDS_ASH_PHONE_HUB_BATTERY_STATUS_CHARGING_AC;
      break;
    case PhoneStatusModel::ChargingState::kChargingUsb:
      charging_tooltip_id = IDS_ASH_PHONE_HUB_BATTERY_STATUS_CHARGING_USB;
      break;
  }
  std::u16string charging_tooltip =
      l10n_util::GetStringUTF16(charging_tooltip_id);

  bool battery_saver_on = phone_status.battery_saver_state() ==
                          PhoneStatusModel::BatterySaverState::kOn;
  std::u16string batter_saver_tooltip =
      battery_saver_on
          ? l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_BATTERY_SAVER_ON)
          : l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_BATTERY_SAVER_OFF);

  battery_icon_->SetTooltipText(
      l10n_util::GetStringFUTF16(IDS_ASH_PHONE_HUB_BATTERY_TOOLTIP,
                                 charging_tooltip, batter_saver_tooltip));
}

void PhoneStatusView::ClearExistingStatus() {
  // Clear mobile status.
  signal_icon_->SetImage(gfx::ImageSkia());

  // Clear battery status.
  battery_icon_->SetImage(gfx::ImageSkia());
  battery_label_->SetText(std::u16string());

  // TODO(b/281844561): When the phone is disconnected the |phone_name_label_|
  // should have cros.sys.disabled. Setting that here and then re-setting the
  // label to cros.sys.on-surface on Update() would handle this case, but it
  // would also incorrectly show cros.sys.disabled for the Connecting and
  // Onboarding UI states.
}

void PhoneStatusView::ConfigureTriViewContainer(TriView::Container container) {
  std::unique_ptr<views::BoxLayout> layout;

  switch (container) {
    case TriView::Container::START:
      SetFlexForContainer(TriView::Container::START, 1.f);

      layout = std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kVertical);
      layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kStretch);
      break;
    case TriView::Container::CENTER:
      layout = std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kStatusSpacing);
      layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kEnd);
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);
      break;
    case TriView::Container::END:
      layout = std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal);
      layout->set_main_axis_alignment(
          views::BoxLayout::MainAxisAlignment::kCenter);
      layout->set_cross_axis_alignment(
          views::BoxLayout::CrossAxisAlignment::kCenter);
      break;
  }

  SetContainerLayout(container, std::move(layout));
  SetMinSize(container, gfx::Size(0, kUnifiedDetailedViewTitleRowHeight));
}

}  // namespace ash
