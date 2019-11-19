// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_NOTIFICATION_MENU_CONTROLLER_H_
#define ASH_APP_MENU_NOTIFICATION_MENU_CONTROLLER_H_

#include "ash/app_menu/app_menu_export.h"
#include "ash/app_menu/notification_menu_view.h"
#include "base/scoped_observer.h"
#include "ui/message_center/message_center.h"
#include "ui/message_center/message_center_observer.h"
#include "ui/views/animation/slide_out_controller_delegate.h"

namespace views {
class MenuItemView;
}

namespace ash {

class AppMenuModelAdapter;

// Handles adding/removing NotificationMenuView from the root MenuItemView,
// adding the container model entry, and updating the NotificationMenuView
// as notifications come and go.
class APP_MENU_EXPORT NotificationMenuController
    : public message_center::MessageCenterObserver,
      public views::SlideOutControllerDelegate,
      public NotificationMenuView::Delegate {
 public:
  NotificationMenuController(const std::string& app_id,
                             views::MenuItemView* root_menu,
                             AppMenuModelAdapter* app_menu_model_adapter);

  ~NotificationMenuController() override;

  // message_center::MessageCenterObserver overrides:
  void OnNotificationAdded(const std::string& notification_id) override;
  void OnNotificationUpdated(const std::string& notification_id) override;
  void OnNotificationRemoved(const std::string& notification_id,
                             bool by_user) override;

  // views::SlideOutControllerDelegate overrides:
  ui::Layer* GetSlideOutLayer() override;
  void OnSlideChanged(bool in_progress) override;
  void OnSlideOut() override;

  // NotificationMenuView::Delegate overrides:
  void OnOverflowAddedOrRemoved() override;
  void ActivateNotificationAndClose(
      const std::string& notification_id) override;

 private:
  // Adds a container MenuItemView to |root_menu_|, adds NOTIFICATION_CONTAINER
  // to |model_|, creates and initializes NotificationMenuView, and adds
  // NotificationMenuView to the container MenuItemView.
  void InitializeNotificationMenuView();

  // Identifies the application the menu is for.
  const std::string app_id_;

  // The top level MenuItemView. Owned by |AppMenuModelAdapter::menu_runner_|.
  views::MenuItemView* const root_menu_;

  // Manages showing the menu. Owned by the view requesting a menu.
  AppMenuModelAdapter* const app_menu_model_adapter_;

  // The view which shows all active notifications for |app_id_|. Owned by the
  // views hierarchy.
  NotificationMenuView* notification_menu_view_ = nullptr;

  ScopedObserver<message_center::MessageCenter,
                 message_center::MessageCenterObserver>
      message_center_observer_;

  DISALLOW_COPY_AND_ASSIGN(NotificationMenuController);
};

}  // namespace ash

#endif  // ASH_APP_MENU_NOTIFICATION_MENU_CONTROLLER_H_
