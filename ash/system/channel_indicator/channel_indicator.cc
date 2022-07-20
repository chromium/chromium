// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator.h"

#include <string>
#include <utility>

#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/style/dark_light_mode_controller.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/tray_item_view.h"
#include "components/session_manager/session_manager_types.h"
#include "components/version_info/channel.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/view.h"

namespace ash {

namespace {

constexpr int kIndicatorBgCornerRadius = 50;

bool IsDisplayableChannel(version_info::Channel channel) {
  switch (channel) {
    case version_info::Channel::BETA:
    case version_info::Channel::DEV:
    case version_info::Channel::CANARY:
      return true;
    default:
      return false;
  }
}

SkColor GetFgColor(version_info::Channel channel) {
  bool is_dark_mode_enabled =
      DarkLightModeController::Get()->IsDarkModeEnabled();
  switch (channel) {
    case version_info::Channel::BETA:
      return is_dark_mode_enabled ? gfx::kGoogleBlue200 : gfx::kGoogleBlue900;
    case version_info::Channel::DEV:
      return is_dark_mode_enabled ? gfx::kGoogleGreen200 : gfx::kGoogleGreen900;
    case version_info::Channel::CANARY:
      return is_dark_mode_enabled ? gfx::kGoogleYellow200 : gfx::kGoogleGrey900;
    default:
      return 0;
  }
}

SkColor GetBgColor(version_info::Channel channel) {
  bool is_dark_mode_enabled =
      DarkLightModeController::Get()->IsDarkModeEnabled();
  switch (channel) {
    case version_info::Channel::BETA:
      return is_dark_mode_enabled ? SkColorSetA(gfx::kGoogleBlue300, 0x55)
                                  : gfx::kGoogleBlue200;
    case version_info::Channel::DEV:
      return is_dark_mode_enabled ? SkColorSetA(gfx::kGoogleGreen300, 0x55)
                                  : gfx::kGoogleGreen200;
    case version_info::Channel::CANARY:
      return is_dark_mode_enabled ? SkColorSetA(gfx::kGoogleYellow300, 0x55)
                                  : gfx::kGoogleYellow200;
    default:
      return 0;
  }
}

int GetStringResource(version_info::Channel channel) {
  DCHECK(IsDisplayableChannel(channel));
  switch (channel) {
    case version_info::Channel::BETA:
      return IDS_ASH_STATUS_TRAY_CHANNEL_BETA;
    case version_info::Channel::DEV:
      return IDS_ASH_STATUS_TRAY_CHANNEL_DEV;
    case version_info::Channel::CANARY:
      return IDS_ASH_STATUS_TRAY_CHANNEL_CANARY;
    default:
      return -1;
  }
}

}  // namespace

ChannelIndicatorView::ChannelIndicatorView(Shelf* shelf,
                                           version_info::Channel channel)
    : TrayItemView(shelf), channel_(channel) {
  SetVisible(false);
  CreateImageView();
  Update(channel_);
}

ChannelIndicatorView::~ChannelIndicatorView() = default;

gfx::Size ChannelIndicatorView::CalculatePreferredSize() const {
  return gfx::Size(kUnifiedTrayChannelIndicatorDimension,
                   kUnifiedTrayChannelIndicatorDimension);
}

void ChannelIndicatorView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->SetName(accessible_name_);
}

views::View* ChannelIndicatorView::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  return GetLocalBounds().Contains(point) ? this : nullptr;
}

std::u16string ChannelIndicatorView::GetTooltipText(const gfx::Point& p) const {
  return tooltip_;
}

const char* ChannelIndicatorView::GetClassName() const {
  return "ChannelIndicatorView";
}

void ChannelIndicatorView::OnThemeChanged() {
  TrayItemView::OnThemeChanged();
  Update(channel_);
}

void ChannelIndicatorView::HandleLocaleChange() {
  Update(channel_);
}

void ChannelIndicatorView::Update(version_info::Channel channel) {
  if (!IsDisplayableChannel(channel))
    return;

  SetVisible(true);
  SetAccessibleName(channel);
  SetTooltip(channel);
  SetImage(channel);
}

void ChannelIndicatorView::SetImage(version_info::Channel channel) {
  DCHECK(IsDisplayableChannel(channel));

  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kUnifiedTrayChannelIndicatorDimension / 2, 0)));
  image_view()->SetBackground(views::CreateRoundedRectBackground(
      GetBgColor(channel), kIndicatorBgCornerRadius));

  switch (channel) {
    case version_info::Channel::BETA:
      image_view()->SetImage(gfx::CreateVectorIcon(
          kChannelBetaIcon, kUnifiedTrayChannelIndicatorDimension,
          GetFgColor(channel)));
      break;
    case version_info::Channel::DEV:
      image_view()->SetImage(gfx::CreateVectorIcon(
          kChannelDevIcon, kUnifiedTrayChannelIndicatorDimension,
          GetFgColor(channel)));
      break;
    case version_info::Channel::CANARY:
      image_view()->SetImage(gfx::CreateVectorIcon(
          kChannelCanaryIcon, kUnifiedTrayChannelIndicatorDimension,
          GetFgColor(channel)));
      break;
    default:
      break;
  }
}

void ChannelIndicatorView::SetAccessibleName(version_info::Channel channel) {
  DCHECK(IsDisplayableChannel(channel));
  accessible_name_ = l10n_util::GetStringUTF16(GetStringResource(channel));
  image_view()->SetAccessibleName(accessible_name_);
}

void ChannelIndicatorView::SetTooltip(version_info::Channel channel) {
  DCHECK(IsDisplayableChannel(channel));
  tooltip_ = l10n_util::GetStringUTF16(GetStringResource(channel));
}

}  // namespace ash
