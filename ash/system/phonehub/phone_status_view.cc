// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/phonehub/phone_status_view.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/style/ash_color_provider.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_popup_item_style.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

constexpr int kTitleContainerSpacing = 16;

views::ImageView* CreateTitleIcon(const gfx::VectorIcon& vector_icon,
                                  int tooltip_text_id) {
  auto* icon = new views::ImageView();
  icon->set_tooltip_text(l10n_util::GetStringUTF16(tooltip_text_id));
  icon->SetImage(CreateVectorIcon(
      vector_icon, AshColorProvider::Get()->GetContentLayerColor(
                       AshColorProvider::ContentLayerType::kIconColorPrimary)));
  return icon;
}

}  // namespace

PhoneStatusView::PhoneStatusView() : TriView(kTitleContainerSpacing) {
  ConfigureTriViewContainer(TriView::Container::START);
  ConfigureTriViewContainer(TriView::Container::CENTER);
  ConfigureTriViewContainer(TriView::Container::END);

  // TODO(leandre): Update title and icons according to phone status.
  auto* title_label = new views::Label(
      l10n_util::GetStringUTF16(IDS_ASH_PHONE_HUB_TRAY_ACCESSIBLE_NAME));
  title_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  TrayPopupItemStyle style(TrayPopupItemStyle::FontStyle::SUB_HEADER,
                           true /* use_unified_theme */);
  style.SetupLabel(title_label);
  AddView(TriView::Container::CENTER, title_label);

  wifi_icon_ = CreateTitleIcon(kUnifiedMenuWifiNoConnectionIcon,
                               IDS_ASH_STATUS_TRAY_NO_NETWORKS);
  AddView(TriView::Container::END, wifi_icon_);

  volume_icon_ = CreateTitleIcon(kSystemMenuVolumeMuteIcon,
                                 IDS_ASH_STATUS_TRAY_VOLUME_STATE_MUTED);
  AddView(TriView::Container::END, volume_icon_);

  battery_icon_ =
      CreateTitleIcon(kBatteryIcon, IDS_ASH_STATUS_TRAY_BATTERY_FULL);
  AddView(TriView::Container::END, battery_icon_);

  settings_button_ = new TopShortcutButton(this, kSystemMenuSettingsIcon,
                                           IDS_ASH_STATUS_TRAY_SETTINGS);
  AddView(TriView::Container::END, settings_button_);
}

PhoneStatusView::~PhoneStatusView() = default;

void PhoneStatusView::ButtonPressed(views::Button* sender,
                                    const ui::Event& event) {
  // TODO(leandre): implement open settings/other buttons.
}

void PhoneStatusView::ConfigureTriViewContainer(TriView::Container container) {
  std::unique_ptr<views::BoxLayout> layout;

  switch (container) {
    case TriView::Container::START:
      FALLTHROUGH;
    case TriView::Container::END:
      layout = std::make_unique<views::BoxLayout>(
          views::BoxLayout::Orientation::kHorizontal, gfx::Insets(),
          kUnifiedTopShortcutSpacing);
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
