// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_tray.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/ash_view_ids.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/notification_center/notification_center_view.h"
#include "ash/system/privacy/privacy_indicators_tray_item_view.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_container.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"

namespace ash {

NotificationCenterTray::NotificationCenterTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kNotificationCenter,
                         RoundedCornerBehavior::kStartRounded),
      notification_icons_controller_(
          std::make_unique<NotificationIconsController>(shelf)) {
  SetID(VIEW_ID_SA_NOTIFICATION_TRAY);
  set_use_bounce_in_animation(false);

  message_center::MessageCenter::Get()->AddObserver(this);

  tray_container()->SetMargin(
      /*main_axis_margin=*/kUnifiedTrayContentPadding -
          ShelfConfig::Get()->status_area_hit_region_padding(),
      0);

  // TODO(b/255986529): Rewrite the `NotificationIconsController` class so that
  // we do not have to add icon views that are owned by the
  // `NotificationCenterTray` from the controller. We should make sure views are
  // only added by host views.
  notification_icons_controller_->AddNotificationTrayItems(tray_container());

  // Do not show this indicator if video conference feature is enabled since
  // privacy indicator is already shown there.
  if (features::IsPrivacyIndicatorsEnabled() &&
      !features::IsVideoConferenceEnabled()) {
    privacy_indicators_view_ = tray_container()->AddChildView(
        std::make_unique<PrivacyIndicatorsTrayItemView>(shelf));
  }
}

NotificationCenterTray::~NotificationCenterTray() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
}

void NotificationCenterTray::OnSystemTrayVisibilityChanged(
    bool system_tray_visible) {
  system_tray_visible_ = system_tray_visible;
  UpdateVisibility();
}

NotificationListView* NotificationCenterTray::GetNotificationListView() {
  return bubble_ ? bubble_->notification_center_view()->notification_list_view()
                 : nullptr;
}

bool NotificationCenterTray::IsBubbleShown() const {
  return !!bubble_;
}

std::u16string NotificationCenterTray::GetAccessibleNameForBubble() {
  return l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_ACCESSIBLE_NAME);
}

std::u16string NotificationCenterTray::GetAccessibleNameForTray() {
  return l10n_util::GetStringUTF16(IDS_ASH_MESSAGE_CENTER_ACCESSIBLE_NAME);
}

void NotificationCenterTray::HandleLocaleChange() {}

void NotificationCenterTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {
  if (bubble_->GetBubbleView() == bubble_view) {
    CloseBubble();
  }
}

void NotificationCenterTray::ClickedOutsideBubble() {
  CloseBubble();
}

void NotificationCenterTray::CloseBubble() {
  if (!bubble_) {
    return;
  }

  bubble_.reset();
  SetIsActive(false);

  // Inform the message center that the bubble has closed so that popups are
  // created for new notifications.
  message_center::MessageCenter::Get()->SetVisibility(
      message_center::VISIBILITY_TRANSIENT);
}

void NotificationCenterTray::ShowBubble() {
  if (bubble_) {
    return;
  }

  // Inform the message center that the bubble is showing so that we do not
  // create popups for incoming notifications and dismiss existing popups. This
  // needs to happen before the bubble is created so that the
  // `NotificationListView` is the active `NotificationViewController` when the
  // `NotificationGroupingController` access it. This happens when notifications
  // are added to the `NotificationListView`.
  message_center::MessageCenter::Get()->SetVisibility(
      message_center::VISIBILITY_MESSAGE_CENTER);

  bubble_ = std::make_unique<NotificationCenterBubble>(this);
  bubble_->ShowBubble();

  SetIsActive(true);
}

void NotificationCenterTray::UpdateAfterLoginStatusChange() {
  UpdateVisibility();
}

TrayBubbleView* NotificationCenterTray::GetBubbleView() {
  return bubble_ ? bubble_->GetBubbleView() : nullptr;
}

views::Widget* NotificationCenterTray::GetBubbleWidget() const {
  return bubble_ ? bubble_->GetBubbleWidget() : nullptr;
}

void NotificationCenterTray::OnAnyBubbleVisibilityChanged(
    views::Widget* bubble_widget,
    bool visible) {
  if (!IsBubbleShown()) {
    return;
  }

  if (bubble_widget == GetBubbleWidget()) {
    return;
  }

  if (visible) {
    // Another bubble is becoming visible while this bubble is being shown, so
    // hide this bubble.
    CloseBubble();
  }
}

void NotificationCenterTray::OnNotificationAdded(
    const std::string& notification_id) {
  UpdateVisibility();
}

void NotificationCenterTray::OnNotificationDisplayed(
    const std::string& notification_id,
    const message_center::DisplaySource source) {
  UpdateVisibility();
}

void NotificationCenterTray::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  UpdateVisibility();
}

void NotificationCenterTray::OnNotificationUpdated(
    const std::string& notification_id) {
  UpdateVisibility();
}

void NotificationCenterTray::UpdateVisibility() {
  const bool new_visibility =
      message_center::MessageCenter::Get()->NotificationCount() > 0 &&
      system_tray_visible_;
  if (new_visibility == visible_preferred()) {
    return;
  }

  SetVisiblePreferred(new_visibility);

  notification_icons_controller_->UpdateNotificationIcons();
  notification_icons_controller_->UpdateNotificationIndicators();

  // We should close the bubble if there are no more notifications to show.
  if (!new_visibility && bubble_) {
    CloseBubble();
  }
}

BEGIN_METADATA(NotificationCenterTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
