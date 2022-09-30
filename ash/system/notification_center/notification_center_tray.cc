// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_tray.h"

#include <string>

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/shelf/shelf.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

NotificationCenterTray::NotificationCenterTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kNotificationCenter,
                         RoundedCornerBehavior::kStartRounded) {
  SetLayoutManager(std::make_unique<views::FlexLayout>());
  set_use_bounce_in_animation(false);

  message_center::MessageCenter::Get()->AddObserver(this);
}

NotificationCenterTray::~NotificationCenterTray() {
  message_center::MessageCenter::Get()->RemoveObserver(this);
}

void NotificationCenterTray::OnSystemTrayVisibilityChanged(
    bool system_tray_visible) {
  system_tray_visible_ = system_tray_visible;
  UpdateVisibility();
}

std::u16string NotificationCenterTray::GetAccessibleNameForTray() {
  return std::u16string();
}

void NotificationCenterTray::HandleLocaleChange() {}

void NotificationCenterTray::HideBubbleWithView(
    const TrayBubbleView* bubble_view) {}

void NotificationCenterTray::ClickedOutsideBubble() {
  CloseBubble();
}

void NotificationCenterTray::CloseBubble() {}

void NotificationCenterTray::ShowBubble() {}

void NotificationCenterTray::UpdateAfterLoginStatusChange() {
  UpdateVisibility();
}

TrayBubbleView* NotificationCenterTray::GetBubbleView() {
  return nullptr;
}

views::Widget* NotificationCenterTray::GetBubbleWidget() const {
  return nullptr;
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
  if (new_visibility == visible_preferred())
    return;

  SetVisiblePreferred(new_visibility);
}

BEGIN_METADATA(NotificationCenterTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
