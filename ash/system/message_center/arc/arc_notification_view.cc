// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/message_center/arc/arc_notification_view.h"

#include <algorithm>

#include "ash/system/message_center/arc/arc_notification_constants.h"
#include "ash/system/message_center/arc/arc_notification_content_view.h"
#include "ash/system/message_center/arc/arc_notification_item.h"
#include "ui/accessibility/ax_action_data.h"
#include "ui/accessibility/ax_enums.mojom.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/gfx/geometry/size.h"
#include "ui/message_center/public/cpp/message_center_constants.h"
#include "ui/message_center/views/notification_background_painter.h"
#include "ui/message_center/views/notification_control_buttons_view.h"
#include "ui/views/background.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/painter.h"

DEFINE_UI_CLASS_PROPERTY_TYPE(ash::ArcNotificationView*)

namespace ash {

DEFINE_UI_CLASS_PROPERTY_KEY(ArcNotificationView*,
                             kArcNotificationViewPropertyKey,
                             nullptr)

// static
ArcNotificationView* ArcNotificationView::FromView(views::View* view) {
  return view->GetProperty(kArcNotificationViewPropertyKey);
}

ArcNotificationView::ArcNotificationView(
    ArcNotificationItem* item,
    const message_center::Notification& notification)
    : message_center::MessageView(notification),
      item_(item),
      content_view_(new ArcNotificationContentView(item_, notification, this)) {
  DCHECK_EQ(message_center::NOTIFICATION_TYPE_CUSTOM, notification.type());
  DCHECK_EQ(kArcNotificationCustomViewType, notification.custom_view_type());

  SetProperty(kArcNotificationViewPropertyKey, this);

  item_->AddObserver(this);

  AddChildView(content_view_);

  if (content_view_->background()) {
    background()->SetNativeControlColor(
        content_view_->background()->get_color());
  }

  focus_painter_ = views::Painter::CreateSolidFocusPainter(
      message_center::kFocusBorderColor, gfx::Insets(0, 1, 3, 2));

  UpdateCornerRadius(message_center::kNotificationCornerRadius,
                     message_center::kNotificationCornerRadius);
}

ArcNotificationView::~ArcNotificationView() {
  if (item_)
    item_->RemoveObserver(this);
}

void ArcNotificationView::OnContentFocused() {
  SchedulePaint();
}

void ArcNotificationView::OnContentBlurred() {
  SchedulePaint();
}

void ArcNotificationView::UpdateWithNotification(
    const message_center::Notification& notification) {
  message_center::MessageView::UpdateWithNotification(notification);
  content_view_->Update(notification);
}

void ArcNotificationView::SetDrawBackgroundAsActive(bool active) {
  // Do nothing if |content_view_| has a background.
  if (content_view_->background())
    return;

  message_center::MessageView::SetDrawBackgroundAsActive(active);
}

void ArcNotificationView::UpdateCornerRadius(int top_radius,
                                             int bottom_radius) {
  MessageView::UpdateCornerRadius(top_radius, bottom_radius);

  content_view_->UpdateCornerRadius(top_radius, bottom_radius);
}

void ArcNotificationView::UpdateControlButtonsVisibility() {
  content_view_->UpdateControlButtonsVisibility();
}

void ArcNotificationView::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  // This data is never used since this view is never focused when the content
  // view is focusable.
}

message_center::NotificationControlButtonsView*
ArcNotificationView::GetControlButtonsView() const {
  return content_view_->GetControlButtonsView();
}

bool ArcNotificationView::IsExpanded() const {
  return item_ && item_->GetExpandState() ==
                      arc::mojom::ArcNotificationExpandState::EXPANDED;
}

bool ArcNotificationView::IsAutoExpandingAllowed() const {
  if (!item_)
    return false;

  // Disallow auto-expanding if the notificaiton is bundled. This is consistent
  // behavior with Android since expanded height of bundle notification might be
  // too long vertically.
  return item_->GetNotificationType() !=
         arc::mojom::ArcNotificationType::BUNDLED;
}

void ArcNotificationView::SetExpanded(bool expanded) {
  if (!item_)
    return;

  auto expand_state = item_->GetExpandState();
  if (expanded) {
    if (expand_state == arc::mojom::ArcNotificationExpandState::COLLAPSED)
      item_->ToggleExpansion();
  } else {
    if (expand_state == arc::mojom::ArcNotificationExpandState::EXPANDED)
      item_->ToggleExpansion();
  }
}

bool ArcNotificationView::IsManuallyExpandedOrCollapsed() const {
  if (item_)
    return item_->IsManuallyExpandedOrCollapsed();
  return false;
}

void ArcNotificationView::OnContainerAnimationStarted() {
  content_view_->OnContainerAnimationStarted();
}

void ArcNotificationView::OnSettingsButtonPressed(const ui::Event& event) {
  MessageView::OnSettingsButtonPressed(event);
}

void ArcNotificationView::OnSnoozeButtonPressed(const ui::Event& event) {
  if (item_)
    return item_->OpenSnooze();
}

void ArcNotificationView::OnContainerAnimationEnded() {
  content_view_->OnContainerAnimationEnded();
}

void ArcNotificationView::OnSlideChanged(bool in_progress) {
  MessageView::OnSlideChanged(in_progress);
  content_view_->OnSlideChanged(in_progress);
}

gfx::Size ArcNotificationView::CalculatePreferredSize() const {
  const gfx::Insets insets = GetInsets();
  const int contents_width = message_center::kNotificationWidth;
  const int contents_height = content_view_->GetHeightForWidth(contents_width);
  return gfx::Size(contents_width + insets.width(),
                   contents_height + insets.height());
}

void ArcNotificationView::Layout() {
  // Setting the bounds before calling the parent to prevent double Layout.
  content_view_->SetBoundsRect(GetContentsBounds());

  message_center::MessageView::Layout();

  // If the content view claims focus, defer focus handling to the content view.
  if (content_view_->IsFocusable())
    SetFocusBehavior(FocusBehavior::NEVER);
}

bool ArcNotificationView::HasFocus() const {
  // In case that focus handling is deferred to the content view, asking the
  // content view about focus.
  return content_view_->IsFocusable() ? content_view_->HasFocus()
                                      : message_center::MessageView::HasFocus();
}

void ArcNotificationView::RequestFocus() {
  if (content_view_->IsFocusable())
    content_view_->RequestFocus();
  else
    message_center::MessageView::RequestFocus();
}

void ArcNotificationView::OnPaint(gfx::Canvas* canvas) {
  MessageView::OnPaint(canvas);
  if (content_view_->IsFocusable()) {
    views::Painter::PaintFocusPainter(content_view_, canvas,
                                      focus_painter_.get());
  }
}

bool ArcNotificationView::OnKeyPressed(const ui::KeyEvent& event) {
  ui::InputMethod* input_method = content_view_->GetInputMethod();
  if (input_method) {
    ui::TextInputClient* text_input_client = input_method->GetTextInputClient();
    if (text_input_client &&
        text_input_client->GetTextInputType() != ui::TEXT_INPUT_TYPE_NONE) {
      // If the focus is in an edit box, we skip the special key handling for
      // back space and return keys. So that these key events are sent to the
      // arc container correctly without being handled by the message center.
      return false;
    }
  }

  return message_center::MessageView::OnKeyPressed(event);
}

void ArcNotificationView::ChildPreferredSizeChanged(View* child) {
  PreferredSizeChanged();
}

bool ArcNotificationView::HandleAccessibleAction(
    const ui::AXActionData& action) {
  if (item_ && action.action == ax::mojom::Action::kDoDefault) {
    item_->ToggleExpansion();
    return true;
  }
  return false;
}

void ArcNotificationView::OnItemDestroying() {
  DCHECK(item_);
  item_->RemoveObserver(this);
  item_ = nullptr;
}

}  // namespace ash
