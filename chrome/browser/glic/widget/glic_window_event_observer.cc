// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/widget/glic_window_event_observer.h"

#include "chrome/browser/glic/widget/glic_view.h"
#include "chrome/browser/glic/widget/glic_widget.h"
#include "chrome/browser/glic/widget/glic_window_animator.h"
#include "ui/events/event_observer.h"
#include "ui/views/event_monitor.h"
#include "ui/views/widget/widget.h"

#if BUILDFLAG(IS_WIN)
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/win/event_creation_utils.h"
#include "ui/display/win/screen_win.h"
#include "ui/views/win/hwnd_util.h"
#endif  // BUILDFLAG(IS_WIN)

namespace glic {

namespace {
constexpr int kDraggableAreaHeight = 44;
}  // namespace

// Helper class for observing mouse and key events from native window.
class GlicWindowEventObserver::WindowEventObserverImpl
    : public ui::EventObserver {
 public:
  WindowEventObserverImpl(GlicWindowEventObserver* observer, GlicView* view)
      : observer_(observer), view_(view) {
    event_monitor_ = views::EventMonitor::CreateWindowMonitor(
        this, view->GetWidget()->GetNativeWindow(),
        {
            ui::EventType::kMousePressed,
            ui::EventType::kMouseReleased,
            ui::EventType::kMouseDragged,
            ui::EventType::kTouchReleased,
            ui::EventType::kTouchPressed,
            ui::EventType::kTouchMoved,
            ui::EventType::kTouchCancelled,
        });
  }

  ~WindowEventObserverImpl() override = default;

  // Determines if the mouse has moved beyond a certain distance to
  // start a drag.
  bool ShouldStartDrag(const gfx::Point& current_mouse_location) {
    // Determine if the mouse has moved beyond a minimum elasticity distance
    // in any direction from the starting point.
    static const int kMinimumDragDistance = 10;
    int x_offset =
        abs(current_mouse_location.x() - initial_click_location_.x());
    int y_offset =
        abs(current_mouse_location.y() - initial_click_location_.y());
    return sqrt(pow(static_cast<float>(x_offset), 2) +
                pow(static_cast<float>(y_offset), 2)) > kMinimumDragDistance;
  }

  void OnEvent(const ui::Event& event) override {
#if BUILDFLAG(IS_WIN)
    if (event.IsTouchEvent()) {
      // If we get a touch event, send the corresponding mouse event so that
      // drag drop of the floaty window will work with touch screens. This is a
      // bit hacky; it would be better to have non client hit tests for the
      // draggable area return HT_CAPTION but that requires the web client to
      // set the draggable areas correctly, and not include the buttons in the
      // titlebar. See crbug.com/388000848.

      const ui::TouchEvent* touch_event = event.AsTouchEvent();
      gfx::Point touch_location = touch_event->location();
      auto touch_screen_point =
          views::View::ConvertPointToScreen(view_, touch_location);
      auto* host = view_->GetWidget()->GetNativeWindow()->GetHost();

      host->ConvertDIPToPixels(&touch_screen_point);
      if (event.type() == ui::EventType::kTouchPressed) {
        POINT cursor_location = touch_screen_point.ToPOINT();
        ::SetCursorPos(cursor_location.x, cursor_location.y);
        touch_down_in_draggable_area_ =
            view_->IsPointWithinDraggableArea(touch_location);
        if (touch_down_in_draggable_area_) {
          ui::SendMouseEvent(touch_screen_point, MOUSEEVENTF_LEFTDOWN);
          ui::SendMouseEvent(touch_screen_point, MOUSEEVENTF_MOVE);
        }
      }
      if (!touch_down_in_draggable_area_) {
        // If we're not in a potential touch drag of the window, ignore touch
        // events.
        return;
      }
      if (event.type() == ui::EventType::kTouchCancelled ||
          event.type() == ui::EventType::kTouchReleased) {
        touch_down_in_draggable_area_ = false;
        ui::SendMouseEvent(touch_screen_point, MOUSEEVENTF_LEFTUP);
      }
      if (event.type() == ui::EventType::kTouchMoved) {
        ui::SendMouseEvent(touch_screen_point, MOUSEEVENTF_MOVE);
      }
      return;
    }
#endif  // BUILDFLAG(IS_WIN)

    gfx::Point mouse_location = event_monitor_->GetLastMouseLocation();
    views::View::ConvertPointFromScreen(view_, &mouse_location);
    if (event.type() == ui::EventType::kMousePressed) {
      mouse_down_in_draggable_area_ =
          view_->IsPointWithinDraggableArea(mouse_location);
      initial_click_location_ = mouse_location;
    }
    if (event.type() == ui::EventType::kMouseReleased ||
        event.type() == ui::EventType::kMouseExited) {
      mouse_down_in_draggable_area_ = false;
      initial_click_location_ = gfx::Point();
    }

    // Window should only be dragged if a corresponding mouse drag event was
    // initiated in the draggable area.
    if (mouse_down_in_draggable_area_ &&
        event.type() == ui::EventType::kMouseDragged &&
        ShouldStartDrag(mouse_location)) {
      observer_->HandleWindowDragWithOffset(
          initial_click_location_.OffsetFromOrigin());
    }
  }

 private:
  raw_ptr<GlicWindowEventObserver> observer_;
  raw_ptr<GlicView> view_;
  std::unique_ptr<views::EventMonitor> event_monitor_;

  // Tracks whether the mouse is pressed and was initially within a draggable
  // area of the window.
  bool mouse_down_in_draggable_area_ = false;

#if BUILDFLAG(IS_WIN)
  // Tracks whether a touch pressed event occurred within the draggable area. If
  // so, subsequent touch events will trigger corresponding mouse events so that
  // window drag works.
  bool touch_down_in_draggable_area_ = false;
#endif  // BUILDFLAG(IS_WIN)

  // Tracks the initial kMousePressed location of a potential drag.
  gfx::Point initial_click_location_;
};

GlicWindowEventObserver::GlicWindowEventObserver(
    base::WeakPtr<GlicWidget> glic_widget,
    Delegate* delegate)
    : widget_(glic_widget), delegate_(delegate) {}

GlicWindowEventObserver::~GlicWindowEventObserver() = default;

void GlicWindowEventObserver::SetDraggingAreasAndWatchForMouseEvents() {
  if (window_event_observer_impl_) {
    return;
  }

  GlicView* glic_view = widget_->GetGlicView();
  if (!glic_view) {
    return;
  }

  window_event_observer_impl_ =
      std::make_unique<WindowEventObserverImpl>(this, glic_view);

  // Set the draggable area to the top bar of the window.
  glic_view->SetDraggableAreas(
      {{0, 0, glic_view->width(), kDraggableAreaHeight}});
}

void GlicWindowEventObserver::HandleWindowDragWithOffset(
    const gfx::Vector2d& mouse_offset) {
  if (!in_move_loop_) {
    in_move_loop_ = true;
    delegate_->window_animator()->CancelAnimation();
#if BUILDFLAG(IS_MAC)
    widget_->SetCapture(nullptr);
#endif
    const views::Widget::MoveLoopSource move_loop_source =
        views::Widget::MoveLoopSource::kMouse;
    widget_->RunMoveLoop(mouse_offset, move_loop_source,
                         views::Widget::MoveLoopEscapeBehavior::kDontHide);
    in_move_loop_ = false;

    delegate_->window_animator()->MaybeAnimateToTargetSize();

    AdjustPositionIfNeeded();
    delegate_->OnDragComplete();
  }
}

void GlicWindowEventObserver::AdjustPositionIfNeeded() {
  // Always have at least `kMinimumVisible` px visible from glic window in
  // both vertical and horizontal directions.
  constexpr int kMinimumVisible = 40;
  const auto widget_size = widget_->GetSize();
  const int horizontal_buffer = widget_size.width() - kMinimumVisible;
  const int vertical_buffer = widget_size.height() - kMinimumVisible;

  // Adjust bounds of visible area screen to allow part of glic to go off
  // screen.
  auto workarea = widget_->GetWorkAreaBoundsInScreen();
  workarea.Outset(gfx::Outsets::VH(vertical_buffer, horizontal_buffer));

  auto rect = widget_->GetRestoredBounds();
  rect.AdjustToFit(workarea);
  widget_->SetBounds(rect);
}

}  // namespace glic
