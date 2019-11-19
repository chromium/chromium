// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_menu/notification_item_view.h"

#include "ash/public/cpp/app_menu_constants.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/text_elider.h"
#include "ui/message_center/views/proportional_image_view.h"
#include "ui/views/animation/slide_out_controller.h"
#include "ui/views/background.h"
#include "ui/views/border.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/layout/box_layout.h"

namespace ash {

namespace {

// Line height of all text in NotificationItemView in dips.
constexpr int kNotificationItemTextLineHeight = 16;

// Padding of |proportional_icon_view_|.
constexpr int kIconVerticalPadding = 4;
constexpr int kIconHorizontalPadding = 12;

// Stroke width of MenuItemView border in dips, used to prevent
// NotificationItemView from exceeding the width of MenuItemView.
constexpr int kBorderStrokeWidth = 1;

// The size of the icon in NotificationItemView in dips.
constexpr gfx::Size kProportionalIconViewSize(24, 24);

// Text color of NotificationItemView's |message_|.
constexpr SkColor kNotificationMessageTextColor =
    SkColorSetARGB(179, 0x5F, 0x63, 0x68);

// Text color of NotificationItemView's |title_|.
constexpr SkColor kNotificationTitleTextColor =
    SkColorSetARGB(230, 0x21, 0x23, 0x24);

}  // namespace

NotificationItemView::NotificationItemView(
    NotificationMenuView::Delegate* delegate,
    views::SlideOutControllerDelegate* slide_out_controller_delegate,
    const base::string16& title,
    const base::string16& message,
    const gfx::Image& icon,
    const std::string& notification_id)
    : delegate_(delegate),
      slide_out_controller_(std::make_unique<views::SlideOutController>(
          this,
          slide_out_controller_delegate)),
      title_(title),
      message_(message),
      notification_id_(notification_id) {
  DCHECK(delegate_);
  DCHECK(slide_out_controller_delegate);

  // Paint to a new layer so |slide_out_controller_| can control the opacity.
  SetPaintToLayer();
  layer()->SetFillsBoundsOpaquely(true);
  SetBorder(views::CreateEmptyBorder(
      gfx::Insets(kNotificationVerticalPadding, kNotificationHorizontalPadding,
                  kNotificationVerticalPadding, kIconHorizontalPadding)));
  SetBackground(views::CreateSolidBackground(SK_ColorWHITE));

  text_container_ = new views::View();
  text_container_->SetLayoutManager(std::make_unique<views::BoxLayout>(
      views::BoxLayout::Orientation::kVertical));
  AddChildView(text_container_);

  title_label_ = new views::Label(title_);
  title_label_->SetEnabledColor(kNotificationTitleTextColor);
  title_label_->SetLineHeight(kNotificationItemTextLineHeight);
  title_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_container_->AddChildView(title_label_);

  message_label_ = new views::Label(message_);
  message_label_->SetEnabledColor(kNotificationMessageTextColor);
  message_label_->SetLineHeight(kNotificationItemTextLineHeight);
  message_label_->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  text_container_->AddChildView(message_label_);

  proportional_icon_view_ =
      new message_center::ProportionalImageView(kProportionalIconViewSize);
  AddChildView(proportional_icon_view_);
  proportional_icon_view_->SetImage(icon.AsImageSkia(),
                                    kProportionalIconViewSize);
}

NotificationItemView::~NotificationItemView() = default;

void NotificationItemView::UpdateContents(const base::string16& title,
                                          const base::string16& message,
                                          const gfx::Image& icon) {
  if (title_ != title) {
    title_ = title;
    title_label_->SetText(title_);
  }
  if (message_ != message) {
    message_ = message;
    message_label_->SetText(message_);
  }
  proportional_icon_view_->SetImage(icon.AsImageSkia(),
                                    kProportionalIconViewSize);
}

gfx::Size NotificationItemView::CalculatePreferredSize() const {
  return gfx::Size(
      views::MenuConfig::instance().touchable_menu_width - kBorderStrokeWidth,
      kNotificationItemViewHeight);
}

void NotificationItemView::Layout() {
  gfx::Insets insets = GetInsets();
  // Enforce |text_container_| width, if necessary the labels will elide as a
  // result of |text_container_| being too small to hold the full width of its
  // children labels.
  const gfx::Size text_container_size(
      views::MenuConfig::instance().touchable_menu_width -
          kNotificationHorizontalPadding - kIconHorizontalPadding * 2 -
          kProportionalIconViewSize.width(),
      title_label_->GetPreferredSize().height() +
          message_label_->GetPreferredSize().height());
  text_container_->SetBounds(insets.left(), insets.top(),
                             text_container_size.width(),
                             text_container_size.height());

  proportional_icon_view_->SetBounds(
      width() - insets.right() - kProportionalIconViewSize.width(),
      insets.top() + kIconVerticalPadding, kProportionalIconViewSize.width(),
      kProportionalIconViewSize.height());
}

bool NotificationItemView::OnMousePressed(const ui::MouseEvent& event) {
  return true;
}

bool NotificationItemView::OnMouseDragged(const ui::MouseEvent& event) {
  return true;
}

void NotificationItemView::OnMouseReleased(const ui::MouseEvent& event) {
  gfx::Point location(event.location());
  views::View::ConvertPointToScreen(this, &location);
  if (!event.IsOnlyLeftMouseButton() ||
      !GetBoundsInScreen().Contains(location)) {
    return;
  }

  delegate_->ActivateNotificationAndClose(notification_id_);
}

void NotificationItemView::OnGestureEvent(ui::GestureEvent* event) {
  // Drag gestures are handled by |slide_out_controller_|.
  switch (event->type()) {
    case ui::ET_GESTURE_TAP:
      event->SetHandled();
      delegate_->ActivateNotificationAndClose(notification_id_);
      return;
    default:
      return;
  }
}

}  // namespace ash
