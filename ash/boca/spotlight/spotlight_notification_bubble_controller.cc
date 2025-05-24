// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/boca/spotlight/spotlight_notification_bubble_controller.h"

#include <memory>
#include <string>

#include "ash/boca/spotlight/spotlight_notification_bubble_view.h"
#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/work_area_insets.h"
#include "ui/base/ui_base_types.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

namespace ash {
namespace {

// Internal name for the Spotlight notification bubble widget. Useful for
// debugging purposes.
constexpr char kSpotlightBubbleWidgetInternalName[] = "SpotlightBubble";
constexpr int kWidgetOnScreenDip = 40;

// Creates the widget for the Spotlight notification view.
std::unique_ptr<views::Widget> CreateBubbleWidget(
    const std::string& widget_name,
    std::unique_ptr<views::View> view) {
  views::Widget::InitParams params(
      views::Widget::InitParams::CLIENT_OWNS_WIDGET,
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.name = widget_name;
  params.activatable = views::Widget::InitParams::Activatable::kDefault;
  params.opacity = views::Widget::InitParams::WindowOpacity::kTranslucent;
  params.z_order = ui::ZOrderLevel::kFloatingUIElement;
  params.visible_on_all_workspaces = true;
  params.parent = Shell::GetContainer(Shell::GetRootWindowForNewWindows(),
                                      kShellWindowId_OverlayContainer);

  auto widget = std::make_unique<views::Widget>();
  widget->Init(std::move(params));
  widget->SetContentsView(std::move(view));
  widget->SetVisibilityAnimationTransition(views::Widget::ANIMATE_BOTH);

  return widget;
}
}  // namespace

SpotlightNotificationBubbleController::SpotlightNotificationBubbleController() =
    default;

SpotlightNotificationBubbleController::
    ~SpotlightNotificationBubbleController() {
  CloseNotificationBubbleNow();
}

void SpotlightNotificationBubbleController::OnEvent(const ui::Event& event) {
  if (!(event.type() == ui::EventType::kMouseEntered)) {
    return;
  }
  switch (location_) {
    case WidgetLocation::kLeft:
      location_ = WidgetLocation::kRight;
      break;
    case WidgetLocation::kRight:
      location_ = WidgetLocation::kLeft;
      break;
  }
  notification_widget_->SetBounds(CalculateWidgetBounds());
}

void SpotlightNotificationBubbleController::ShowNotificationBubble(
    const std::string& teacher_name) {
  if (notification_widget_) {
    return;
  }

  auto spotlight_notification_bubble_view =
      std::make_unique<SpotlightNotificationBubbleView>(teacher_name);
  auto* spotlight_notification_bubble_view_ptr =
      spotlight_notification_bubble_view.get();
  notification_widget_ =
      CreateBubbleWidget(kSpotlightBubbleWidgetInternalName,
                         std::move(spotlight_notification_bubble_view));
  notification_widget_->SetBounds(CalculateWidgetBounds());
  notification_widget_->Hide();
  event_monitor_ = views::EventMonitor::CreateWindowMonitor(
      /*event_observer=*/this, notification_widget_->GetNativeWindow(),
      {ui::EventType::kMouseEntered});

  spotlight_notification_bubble_view_ptr->ShowInactive();
  notification_widget_->SetSize(
      spotlight_notification_bubble_view_ptr->GetPreferredSize());
  notification_widget_->SetBounds(CalculateWidgetBounds());
}

void SpotlightNotificationBubbleController::HideNotificationBubble() {
  if (!notification_widget_) {
    return;
  }
  event_monitor_.reset();
  notification_widget_->Hide();
  notification_widget_.reset();
}

bool SpotlightNotificationBubbleController::IsNotificationBubbleVisible() {
  if (!notification_widget_) {
    return false;
  }
  return notification_widget_->IsVisible();
}

void SpotlightNotificationBubbleController::OnSessionEnded() {
  CloseNotificationBubbleNow();
}

const gfx::Rect SpotlightNotificationBubbleController::CalculateWidgetBounds() {
  gfx::Rect ash_window_bounds =
      WorkAreaInsets::ForWindow(ash::Shell::GetRootWindowForNewWindows())
          ->user_work_area_bounds();
  ash_window_bounds.Inset(gfx::Insets(kWidgetOnScreenDip));
  const gfx::Size preferred_size =
      notification_widget_->GetContentsView()->GetPreferredSize();

  gfx::Point origin;
  switch (location_) {
    case WidgetLocation::kLeft:
      origin = gfx::Point(ash_window_bounds.x(),
                          ash_window_bounds.bottom() - preferred_size.height());
      break;
    case WidgetLocation::kRight:
      origin = gfx::Point(ash_window_bounds.right() - preferred_size.width(),
                          ash_window_bounds.bottom() - preferred_size.height());
      break;
  }
  return gfx::Rect(origin, preferred_size);
}

void SpotlightNotificationBubbleController::CloseNotificationBubbleNow() {
  if (!notification_widget_) {
    return;
  }
  event_monitor_.reset();
  notification_widget_->CloseNow();
  notification_widget_.reset();
}

}  // namespace ash
