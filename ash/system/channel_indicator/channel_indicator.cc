// Copyright (c) 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/channel_indicator/channel_indicator.h"

#include "ash/resources/vector_icons/vector_icons.h"
#include "ash/shelf/shelf.h"
#include "ash/system/channel_indicator/channel_indicator_utils.h"
#include "ash/system/tray/tray_constants.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/paint_vector_icon.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/image_view.h"

namespace ash {

namespace {

constexpr int kIndicatorBgCornerRadius = 50;

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
  if (!channel_indicator_utils::IsDisplayableChannel(channel))
    return;

  SetVisible(true);
  SetAccessibleName(channel);
  SetTooltip(channel);
  SetImage(channel);
}

void ChannelIndicatorView::SetImage(version_info::Channel channel) {
  DCHECK(channel_indicator_utils::IsDisplayableChannel(channel));

  SetBorder(views::CreateEmptyBorder(
      gfx::Insets::VH(kUnifiedTrayChannelIndicatorDimension / 2, 0)));
  image_view()->SetBackground(views::CreateRoundedRectBackground(
      channel_indicator_utils::GetBgColor(channel), kIndicatorBgCornerRadius));

  switch (channel) {
    case version_info::Channel::BETA:
      image_view()->SetImage(gfx::CreateVectorIcon(
          kChannelBetaIcon, kUnifiedTrayChannelIndicatorDimension,
          channel_indicator_utils::GetFgColor(channel)));
      break;
    case version_info::Channel::DEV:
      image_view()->SetImage(gfx::CreateVectorIcon(
          kChannelDevIcon, kUnifiedTrayChannelIndicatorDimension,
          channel_indicator_utils::GetFgColor(channel)));
      break;
    case version_info::Channel::CANARY:
      image_view()->SetImage(gfx::CreateVectorIcon(
          kChannelCanaryIcon, kUnifiedTrayChannelIndicatorDimension,
          channel_indicator_utils::GetFgColor(channel)));
      break;
    default:
      break;
  }
}

void ChannelIndicatorView::SetAccessibleName(version_info::Channel channel) {
  DCHECK(channel_indicator_utils::IsDisplayableChannel(channel));
  accessible_name_ = l10n_util::GetStringUTF16(
      channel_indicator_utils::GetChannelNameStringResourceID(channel, true));
  image_view()->SetAccessibleName(accessible_name_);
}

void ChannelIndicatorView::SetTooltip(version_info::Channel channel) {
  DCHECK(channel_indicator_utils::IsDisplayableChannel(channel));
  tooltip_ = l10n_util::GetStringUTF16(
      channel_indicator_utils::GetChannelNameStringResourceID(channel, true));
}

}  // namespace ash
