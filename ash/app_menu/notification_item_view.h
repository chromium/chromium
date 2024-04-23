// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_MENU_NOTIFICATION_ITEM_VIEW_H_
#define ASH_APP_MENU_NOTIFICATION_ITEM_VIEW_H_

#include <memory>
#include <string>

#include "ash/app_menu/app_menu_export.h"
#include "ash/app_menu/notification_menu_view.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/views/animation/slide_out_controller_delegate.h"
#include "ui/views/view.h"

namespace gfx {
class Size;
}  // namespace gfx

namespace message_center {
class ProportionalImageView;
}

namespace views {
class Label;
class SlideOutController;
}

namespace ui {
class ImageModel;
}

namespace ash {

// The view which contains the details of a notification.
class APP_MENU_EXPORT NotificationItemView : public views::View {
  METADATA_HEADER(NotificationItemView, views::View)

 public:
  NotificationItemView(
      NotificationMenuView::Delegate* delegate,
      views::SlideOutControllerDelegate* slide_out_controller_delegate,
      const std::u16string& title,
      const std::u16string& message,
      const ui::ImageModel& icon,
      const std::string& notification_id);

  NotificationItemView(const NotificationItemView&) = delete;
  NotificationItemView& operator=(const NotificationItemView&) = delete;

  ~NotificationItemView() override;

  // Updates the contents of the view.
  void UpdateContents(const std::u16string& title,
                      const std::u16string& message,
                      const ui::ImageModel& icon);

  // views::View overrides:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  const std::string& notification_id() const { return notification_id_; }
  const std::u16string& title() const { return title_; }
  const std::u16string& message() const { return message_; }
  const message_center::ProportionalImageView& proportional_image_view() const {
    return *proportional_icon_view_;
  }

 private:
  // Holds the title and message labels. Owned by the views hierarchy.
  raw_ptr<views::View> text_container_ = nullptr;

  // Holds the notification's icon. Owned by the views hierarchy.
  raw_ptr<message_center::ProportionalImageView> proportional_icon_view_ =
      nullptr;

  // Shows the title, owned by the views hierarchy.
  raw_ptr<views::Label> title_label_ = nullptr;

  // Shows the message, owned by the views hierarchy.
  raw_ptr<views::Label> message_label_ = nullptr;

  // Owned by AppMenuModelAdapter. Used to activate notifications.
  const raw_ptr<NotificationMenuView::Delegate, DanglingUntriaged> delegate_;

  // Controls the sideways gesture drag behavior.
  std::unique_ptr<views::SlideOutController> slide_out_controller_;

  // Notification properties.
  std::u16string title_;
  std::u16string message_;

  // The identifier used by MessageCenter to identify this notification.
  const std::string notification_id_;
};

}  // namespace ash

#endif  // ASH_APP_MENU_NOTIFICATION_ITEM_VIEW_H_
