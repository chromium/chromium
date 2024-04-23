// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_NOTIFICATION_MENU_VIEW_H_
#define ASH_APP_MENU_NOTIFICATION_MENU_VIEW_H_

#include <deque>
#include <string>

#include "ash/app_menu/app_menu_export.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/view.h"

namespace message_center {
class Notification;
}

namespace views {
class MenuSeparator;
class SlideOutControllerDelegate;
}

namespace ash {

class NotificationMenuHeaderView;
class NotificationOverflowView;
class NotificationItemView;

// A view inserted into a container MenuItemView which shows a
// NotificationItemView and a NotificationMenuHeaderView.
class APP_MENU_EXPORT NotificationMenuView : public views::View {
  METADATA_HEADER(NotificationMenuView, views::View)

 public:
  // API for child views to interact with the NotificationMenuController.
  class Delegate {
   public:
    virtual ~Delegate() = default;
    // Activates the notification corresponding with |notification_id| and
    // closes the menu.
    virtual void ActivateNotificationAndClose(
        const std::string& notification_id) = 0;

    // Called when an overflow view is added or remove.
    virtual void OnOverflowAddedOrRemoved() = 0;
  };

  NotificationMenuView(
      Delegate* notification_item_view_delegate,
      views::SlideOutControllerDelegate* slide_out_controller_delegate,
      const std::string& app_id);

  NotificationMenuView(const NotificationMenuView&) = delete;
  NotificationMenuView& operator=(const NotificationMenuView&) = delete;

  ~NotificationMenuView() override;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;

  // Whether |notifications_for_this_app_| is empty.
  bool IsEmpty() const;

  // Adds |notification| as a NotificationItemView, displacing the currently
  // displayed NotificationItemView, if it exists.
  void AddNotificationItemView(
      const message_center::Notification& notification);

  // Updates the NotificationItemView corresponding to |notification|, replacing
  // the contents if they have changed.
  void UpdateNotificationItemView(
      const message_center::Notification& notification);

  // Removes the NotificationItemView associated with |notification_id| and
  // if it is the currently displayed NotificationItemView, replaces it with
  // the next one if available. Also removes the notification from
  // |overflow_view_| if it exists there.
  void OnNotificationRemoved(const std::string& notification_id);

  // Gets the slide out layer, used to move the displayed NotificationItemView.
  ui::Layer* GetSlideOutLayer();

  // Returns the currently-visible notification, or null if none.
  const NotificationItemView* GetDisplayedNotificationItemView() const;
  NotificationItemView* GetDisplayedNotificationItemView() {
    return const_cast<NotificationItemView*>(
        static_cast<const NotificationMenuView*>(this)
            ->GetDisplayedNotificationItemView());
  }

  // Gets the notification id of the displayed NotificationItemView.
  const std::string& GetDisplayedNotificationID() const;

 private:
  friend class NotificationMenuViewTestAPI;

  using NotificationItemViews = std::deque<NotificationItemView*>;

  // Returns an iterator to the notification matching the supplied ID, or
  // notification_item_views_.end() if none.
  NotificationItemViews::iterator NotificationIterForId(const std::string& id);

  // Identifies the app for this menu.
  const std::string app_id_;

  // Owned by AppMenuModelAdapter.
  const raw_ptr<NotificationMenuView::Delegate, DanglingUntriaged>
      notification_item_view_delegate_;

  // Owned by AppMenuModelAdapter.
  const raw_ptr<views::SlideOutControllerDelegate, DanglingUntriaged>
      slide_out_controller_delegate_;

  // The deque of NotificationItemViews. The front item in the deque is the view
  // which is shown.
  NotificationItemViews notification_item_views_;

  // A double separator used to distinguish notifications from context menu
  // options. Owned by views hierarchy.
  raw_ptr<views::MenuSeparator, DanglingUntriaged> double_separator_;

  // Holds the header and counter texts. Owned by views hierarchy.
  raw_ptr<NotificationMenuHeaderView, DanglingUntriaged> header_view_;

  // A view that shows icons of notifications for this app that are not being
  // shown.
  raw_ptr<NotificationOverflowView, DanglingUntriaged> overflow_view_ = nullptr;
};

}  // namespace ash

#endif  // ASH_APP_MENU_NOTIFICATION_MENU_VIEW_H_
