// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_NOTIFICATION_ITEM_VIEW_H_
#define ASH_APP_MENU_NOTIFICATION_ITEM_VIEW_H_

#include <memory>
#include <string>

#include "ash/app_menu/app_menu_export.h"
#include "ash/app_menu/notification_menu_view.h"
#include "base/strings/string16.h"
#include "ui/views/animation/slide_out_controller_delegate.h"
#include "ui/views/view.h"

namespace gfx {
class Image;
class Size;
}  // namespace gfx

namespace message_center {
class ProportionalImageView;
}

namespace views {
class Label;
class SlideOutController;
}

namespace ash {

// The view which contains the details of a notification.
class APP_MENU_EXPORT NotificationItemView : public views::View {
 public:
  NotificationItemView(
      NotificationMenuView::Delegate* delegate,
      views::SlideOutControllerDelegate* slide_out_controller_delegate,
      const base::string16& title,
      const base::string16& message,
      const gfx::Image& icon,
      const std::string& notification_id);

  ~NotificationItemView() override;

  // Updates the contents of the view.
  void UpdateContents(const base::string16& title,
                      const base::string16& message,
                      const gfx::Image& icon);

  // views::View overrides:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  const std::string& notification_id() const { return notification_id_; }
  const base::string16& title() const { return title_; }
  const base::string16& message() const { return message_; }
  const message_center::ProportionalImageView& proportional_image_view() const {
    return *proportional_icon_view_;
  }

 private:
  // Holds the title and message labels. Owned by the views hierarchy.
  views::View* text_container_ = nullptr;

  // Holds the notification's icon. Owned by the views hierarchy.
  message_center::ProportionalImageView* proportional_icon_view_ = nullptr;

  // Shows the title, owned by the views hierarchy.
  views::Label* title_label_ = nullptr;

  // Shows the message, owned by the views hierarchy.
  views::Label* message_label_ = nullptr;

  // Owned by AppMenuModelAdapter. Used to activate notifications.
  NotificationMenuView::Delegate* const delegate_;

  // Controls the sideways gesture drag behavior.
  std::unique_ptr<views::SlideOutController> slide_out_controller_;

  // Notification properties.
  base::string16 title_;
  base::string16 message_;

  // The identifier used by MessageCenter to identify this notification.
  const std::string notification_id_;

  DISALLOW_COPY_AND_ASSIGN(NotificationItemView);
};

}  // namespace ash

#endif  // ASH_APP_MENU_NOTIFICATION_ITEM_VIEW_H_
