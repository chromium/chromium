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
#include "ash/system/model/update_model.h"
#include "ash/system/unified/buttons.h"
#include "ash/system/unified/unified_system_tray_controller.h"
#include "ash/system/update/eol_notice_quick_settings_view.h"
#include "components/session_manager/session_manager_types.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/layout/box_layout.h"

namespace ash {
namespace {

// The bottom padding is 0 so this view is flush with the feature tiles.
constexpr auto kHeaderPadding = gfx::Insets::TLBR(16, 16, 0, 16);

// Horizontal space between header buttons.
constexpr int kButtonSpacing = 8;

// Header button size when the button is narrow (e.g. two column layout).
constexpr gfx::Size kNarrowButtonSize(180, 32);

// Header button size when the button is wide (e.g. one column layout).
constexpr gfx::Size kWideButtonSize(408, 32);

}  // namespace

QuickSettingsHeader::QuickSettingsHeader(
    UnifiedSystemTrayController* controller) {
  DCHECK(features::IsQsRevampEnabled());

  SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kHorizontal, kHeaderPadding,
      kButtonSpacing));

  enterprise_managed_view_ =
      AddChildView(std::make_unique<EnterpriseManagedView>(controller));

  supervised_view_ = AddChildView(std::make_unique<SupervisedUserView>());

  if (Shell::Get()->session_controller()->GetSessionState() ==
      session_manager::SessionState::ACTIVE) {
    if (Shell::Get()->system_tray_model()->update_model()->show_eol_notice()) {
      eol_notice_ =
          AddChildView(std::make_unique<EolNoticeQuickSettingsView>());
    }

    // If the release track is not "stable" then show the channel indicator UI.
    auto channel = Shell::Get()->shell_delegate()->GetChannel();
    if (channel_indicator_utils::IsDisplayableChannel(channel) &&
        !eol_notice_) {
      channel_view_ =
          AddChildView(std::make_unique<ChannelIndicatorQuickSettingsView>(
              channel, Shell::Get()
                           ->system_tray_model()
                           ->client()
                           ->IsUserFeedbackEnabled()));
    }
  }

  UpdateVisibilityAndLayout();
}

QuickSettingsHeader::~QuickSettingsHeader() = default;

void QuickSettingsHeader::ChildVisibilityChanged(views::View* child) {
  UpdateVisibilityAndLayout();
}

void QuickSettingsHeader::UpdateVisibilityAndLayout() {
  // The managed view and the supervised view are never shown together.
  DCHECK(!enterprise_managed_view_->GetVisible() ||
         !supervised_view_->GetVisible());

  // Make `this` view visible if a child is visible.
  bool managed_view_visible =
      enterprise_managed_view_->GetVisible() || supervised_view_->GetVisible();
  bool channel_view_visible = !!channel_view_;
  bool eol_notice_visible = !!eol_notice_;

  SetVisible(managed_view_visible || channel_view_visible ||
             eol_notice_visible);

  // Update button sizes for one column vs. two columns.
  bool two_columns =
      managed_view_visible && (channel_view_visible || eol_notice_visible);
  gfx::Size size = two_columns ? kNarrowButtonSize : kWideButtonSize;
  enterprise_managed_view_->SetPreferredSize(size);
  supervised_view_->SetPreferredSize(size);
  if (channel_view_)
    channel_view_->SetPreferredSize(size);
  if (eol_notice_) {
    eol_notice_->SetPreferredSize(size);
  }

  // Use custom narrow layouts when two columns are showing.
  enterprise_managed_view_->SetNarrowLayout(two_columns);
  if (channel_view_) {
    channel_view_->SetNarrowLayout(two_columns);
  }
  if (eol_notice_) {
    eol_notice_->SetNarrowLayout(two_columns);
  }
}

BEGIN_METADATA(QuickSettingsHeader, views::View)
END_METADATA

}  // namespace ash
