// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_menu_controller.h"

#include "ash/app_menu/app_menu_model_adapter.h"
#include "ash/app_menu/notification_menu_view.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace ash {

NotificationMenuController::NotificationMenuController(
    const std::string& app_id,
    views::MenuItemView* root_menu,
    AppMenuModelAdapter* app_menu_model_adapter)
    : app_id_(app_id),
      root_menu_(root_menu),
      app_menu_model_adapter_(app_menu_model_adapter),
      message_center_observer_(this) {
  DCHECK(app_menu_model_adapter_);
  message_center_observer_.Add(message_center::MessageCenter::Get());
  InitializeNotificationMenuView();
}

NotificationMenuController::~NotificationMenuController() = default;

void NotificationMenuController::OnNotificationAdded(
    const std::string& notification_id) {
  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);

  DCHECK(notification);

  if (notification->notifier_id().id != app_id_)
    return;

  if (!notification_menu_view_) {
    InitializeNotificationMenuView();
    return;
  }

  notification_menu_view_->AddNotificationItemView(*notification);
}

void NotificationMenuController::OnNotificationUpdated(
    const std::string& notification_id) {
  if (!notification_menu_view_)
    return;

  message_center::Notification* notification =
      message_center::MessageCenter::Get()->FindVisibleNotificationById(
          notification_id);

  DCHECK(notification);
  if (notification->notifier_id().id != app_id_)
    return;

  notification_menu_view_->UpdateNotificationItemView(*notification);
}

void NotificationMenuController::OnNotificationRemoved(
    const std::string& notification_id,
    bool by_user) {
  if (!notification_menu_view_)
    return;

  // Remove the view from the container.
  notification_menu_view_->OnNotificationRemoved(notification_id);

  if (!notification_menu_view_->IsEmpty())
    return;

  // There are no more notifications to show, so remove |item_| from
  // |root_menu_|, and remove the entry from the model.
  root_menu_->RemoveMenuItem(notification_menu_view_->parent());
  app_menu_model_adapter_->model()->RemoveItemAt(
      app_menu_model_adapter_->model()->GetIndexOfCommandId(
          NOTIFICATION_CONTAINER));
  notification_menu_view_ = nullptr;

  // Notify the root MenuItemView so it knows to resize and re-calculate the
  // menu bounds.
  root_menu_->ChildrenChanged();
}

ui::Layer* NotificationMenuController::GetSlideOutLayer() {
  return notification_menu_view_ ? notification_menu_view_->GetSlideOutLayer()
                                 : nullptr;
}

void NotificationMenuController::OnSlideChanged(bool in_progress) {}

void NotificationMenuController::OnSlideOut() {
  // Results in |this| being deleted if there are no more notifications to show.
  // Only the displayed NotificationItemView can call OnSlideOut.
  message_center::MessageCenter::Get()->RemoveNotification(
      notification_menu_view_->GetDisplayedNotificationID(), true);
}

void NotificationMenuController::ActivateNotificationAndClose(
    const std::string& notification_id) {
  message_center::MessageCenter::Get()->ClickOnNotification(notification_id);

  // Results in |this| being deleted.
  app_menu_model_adapter_->Cancel();
}

void NotificationMenuController::OnOverflowAddedOrRemoved() {
  // Make the root MenuItemView recalculate the menu bounds.
  root_menu_->ChildrenChanged();
}

void NotificationMenuController::InitializeNotificationMenuView() {
  DCHECK(!notification_menu_view_);

  // Initialize the container only if there are notifications to show.
  if (message_center::MessageCenter::Get()
          ->FindNotificationsByAppId(app_id_)
          .empty()) {
    return;
  }

  app_menu_model_adapter_->model()->AddItem(NOTIFICATION_CONTAINER,
                                            base::string16());
  // Add the container MenuItemView to |root_menu_|.
  views::MenuItemView* container =
      root_menu_->AppendMenuItem(NOTIFICATION_CONTAINER);
  notification_menu_view_ = new NotificationMenuView(this, this, app_id_);
  container->AddChildView(notification_menu_view_);

  for (auto* notification :
       message_center::MessageCenter::Get()->FindNotificationsByAppId(
           app_id_)) {
    notification_menu_view_->AddNotificationItemView(*notification);
  }

  // Notify the root MenuItemView so it knows to resize and re-calculate the
  // menu bounds.
  root_menu_->ChildrenChanged();
}

}  // namespace ash
