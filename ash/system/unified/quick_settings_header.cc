// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/unified/quick_settings_header.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/system/channel_indicator/channel_indicator_quick_settings_view.h"
#include "ash/system/channel_indicator/channel_indicator_utils.h"
#include "ash/system/model/system_tray_model.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// The bottom padding is 0 so this view is flush with the feature tiles.
constexpr auto kHeaderPadding = gfx::Insets::TLBR(16, 16, 0, 16);

constexpr int kBetweenChildSpacing = 8;

}  // namespace

QuickSettingsHeader::QuickSettingsHeader() {
  DCHECK(features::IsQsRevampEnabled());

  auto* layout = SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical, kHeaderPadding,
      kBetweenChildSpacing));
  layout->set_cross_axis_alignment(
      views::BoxLayout::CrossAxisAlignment::kStretch);

  // If the release track is not "stable" then show the channel indicator UI.
  auto channel = Shell::Get()->shell_delegate()->GetChannel();
  if (channel_indicator_utils::IsDisplayableChannel(channel) &&
      Shell::Get()->session_controller()->GetSessionState() ==
          session_manager::SessionState::ACTIVE) {
    channel_view_ =
        AddChildView(std::make_unique<ChannelIndicatorQuickSettingsView>(
            channel, Shell::Get()
                         ->system_tray_model()
                         ->client()
                         ->IsUserFeedbackEnabled()));
  }

  UpdateVisibility();
}

QuickSettingsHeader::~QuickSettingsHeader() = default;

void QuickSettingsHeader::UpdateVisibility() {
  // TODO(b/251724754): Update condition when enterprise management view is
  // added.
  bool should_show = !!channel_view_;
  SetVisible(should_show);
}

BEGIN_METADATA(QuickSettingsHeader, views::View)
END_METADATA

}  // namespace ash
