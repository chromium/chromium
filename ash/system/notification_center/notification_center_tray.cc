// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/notification_center/notification_center_tray.h"

#include <string>

#include "ash/constants/tray_background_view_catalog.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/shelf/shelf.h"
#include "ash/system/notification_center/notification_center_bubble.h"
#include "ash/system/tray/tray_background_view.h"
#include "ash/system/tray/tray_bubble_view.h"
#include "ash/system/tray/tray_container.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_types.h"
#include "ui/views/layout/flex_layout.h"

namespace ash {

NotificationCenterTray::NotificationCenterTray(Shelf* shelf)
    : TrayBackgroundView(shelf,
                         TrayBackgroundViewCatalogName::kNotificationCenter,
                         RoundedCornerBehavior::kStartRounded),
      notification_icons_controller_(
          std::make_unique<NotificationIconsController>(shelf)) {
  SetLayoutManager(std::make_unique<views::FlexLayout>());
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
}

NotificationCenterTray::~NotificationCenterTray() {
  if (GetBubbleWidget())
    GetBubbleWidget()->RemoveObserver(this);

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

void NotificationCenterTray::CloseBubble() {
  if (!bubble_)
    return;

  if (GetBubbleWidget())
    GetBubbleWidget()->RemoveObserver(this);

  bubble_.reset();
  SetIsActive(false);
}

void NotificationCenterTray::ShowBubble() {
  if (bubble_)
    return;

  bubble_ = std::make_unique<NotificationCenterBubble>(this);

  // Observe the bubble widget so that we can do proper clean up when it is
  // being destroyed. If destruction is due to a call to `CloseBubble()` we will
  // have already cleaned up state but there are cases where the bubble widget
  // is destroyed independent of a call to `CloseBubble()`, e.g. ESC key press.
  GetBubbleWidget()->AddObserver(this);

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

// We need to call `CloseBubble()` explicitly if the bubble's widget is
// destroyed independently of `CloseBubble()` e.g. ESC key press. The bubble
// needs to be cleaned up here since it is owned by `NotificationCenterTray`.
void NotificationCenterTray::OnWidgetDestroying(views::Widget* widget) {
  CloseBubble();
}

void NotificationCenterTray::UpdateVisibility() {
  const bool new_visibility =
      message_center::MessageCenter::Get()->NotificationCount() > 0 &&
      system_tray_visible_;
  if (new_visibility == visible_preferred())
    return;

  SetVisiblePreferred(new_visibility);

  notification_icons_controller_->UpdateNotificationIcons();
  notification_icons_controller_->UpdateNotificationIndicators();

  // We should close the bubble if there are no more notifications to show.
  if (!new_visibility && bubble_)
    CloseBubble();
}

BEGIN_METADATA(NotificationCenterTray, TrayBackgroundView)
END_METADATA

}  // namespace ash
