// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/drag_drop/drag_drop_tracker.h"

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/shell.h"
#include "ash/wm/window_util.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/aura/client/window_parenting_client.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/base/hit_test.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/wm/core/coordinate_conversion.h"
#include "ui/wm/public/activation_delegate.h"

namespace ash {

class DragDropTrackerDelegate : public aura::WindowDelegate {
 public:
  explicit DragDropTrackerDelegate(CancelDragDropCallback callback)
      : cancel_callback_(callback) {}

  DragDropTrackerDelegate(const DragDropTrackerDelegate&) = delete;
  DragDropTrackerDelegate& operator=(const DragDropTrackerDelegate&) = delete;

  ~DragDropTrackerDelegate() override = default;

  // Overridden from WindowDelegate:
  gfx::Size GetMinimumSize() const override { return gfx::Size(); }
  gfx::Size GetMaximumSize() const override { return gfx::Size(); }
  void OnBoundsChanged(const gfx::Rect& old_bounds,
                       const gfx::Rect& new_bounds) override {}
  gfx::NativeCursor GetCursor(const gfx::Point& point) override {
    return gfx::NativeCursor{};
  }
  int GetNonClientComponent(const gfx::Point& point) const override {
    return HTCAPTION;
  }
  bool ShouldDescendIntoChildForEventHandling(
      aura::Window* child,
      const gfx::Point& location) override {
    return true;
  }
  bool CanFocus() override { return true; }
  void OnCaptureLost() override { cancel_callback_.Run(); }
  void OnPaint(const ui::PaintContext& context) override {}
  void OnDeviceScaleFactorChanged(float old_device_scale_factor,
                                  float new_device_scale_factor) override {}
  void OnWindowDestroying(aura::Window* window) override {}
  void OnWindowDestroyed(aura::Window* window) override {}
  void OnWindowTargetVisibilityChanged(bool visible) override {}
  bool HasHitTestMask() const override { return true; }
  void GetHitTestMask(SkPath* mask) const override { DCHECK(mask->isEmpty()); }

 private:
  CancelDragDropCallback cancel_callback_;
};

namespace {

// An activation delegate which disables activating the drag and drop window.
class CaptureWindowActivationDelegate : public ::wm::ActivationDelegate {
 public:
  CaptureWindowActivationDelegate() = default;

  CaptureWindowActivationDelegate(const CaptureWindowActivationDelegate&) =
      delete;
  CaptureWindowActivationDelegate& operator=(
      const CaptureWindowActivationDelegate&) = delete;

  ~CaptureWindowActivationDelegate() override = default;

  // wm::ActivationDelegate overrides:
  bool ShouldActivate() const override { return false; }
};

// Creates a window for capturing drag events.
std::unique_ptr<aura::Window> CreateCaptureWindow(
    aura::Window* context_root,
    aura::WindowDelegate* delegate) {
  static CaptureWindowActivationDelegate* activation_delegate_instance =
      new CaptureWindowActivationDelegate();
  auto window = std::make_unique<aura::Window>(delegate);
  // Set type of window as popup to prevent different window manager codes
  // trying to manage this window.
  window->SetType(aura::client::WINDOW_TYPE_POPUP);
  window->Init(ui::LAYER_NOT_DRAWN);
  aura::client::ParentWindowWithContext(window.get(), context_root, gfx::Rect(),
                                        display::kInvalidDisplayId);
  ::wm::SetActivationDelegate(window.get(), activation_delegate_instance);
  window->Show();
  DCHECK(window->bounds().size().IsEmpty());
  return window;
}

}  // namespace

DragDropTracker::DragDropTracker(aura::Window* context_root,
                                 CancelDragDropCallback callback)
    : tracker_window_delegate_(new DragDropTrackerDelegate(callback)),
      capture_window_(
          CreateCaptureWindow(context_root, tracker_window_delegate_.get())) {}

DragDropTracker::~DragDropTracker() {
  capture_window_->ReleaseCapture();
}

void DragDropTracker::TakeCapture() {
  capture_window_->SetCapture();
}

aura::Window* DragDropTracker::GetTarget(const ui::LocatedEvent& event) {
  DCHECK(capture_window_.get());
  gfx::Point location_in_screen = event.location();
  ::wm::ConvertPointToScreen(capture_window_.get(), &location_in_screen);
  aura::Window* root_window_at_point =
      window_util::GetRootWindowAt(location_in_screen);
  gfx::Point location_in_root = location_in_screen;
  ::wm::ConvertPointFromScreen(root_window_at_point, &location_in_root);
  return root_window_at_point->GetEventHandlerForPoint(location_in_root);
}

std::unique_ptr<ui::LocatedEvent> DragDropTracker::ConvertEvent(
    aura::Window* target,
    const ui::LocatedEvent& event) {
  DCHECK(capture_window_.get());
  gfx::Point target_location = event.location();
  aura::Window::ConvertPointToTarget(capture_window_.get(), target,
                                     &target_location);
  gfx::Point location_in_screen = event.location();
  ::wm::ConvertPointToScreen(capture_window_.get(), &location_in_screen);
  gfx::Point target_root_location = event.root_location();
  aura::Window::ConvertPointToTarget(
      capture_window_->GetRootWindow(),
      window_util::GetRootWindowAt(location_in_screen), &target_root_location);
  int changed_button_flags = 0;
  if (event.IsMouseEvent())
    changed_button_flags = event.AsMouseEvent()->changed_button_flags();
  return std::make_unique<ui::MouseEvent>(
      event.type(), target_location, target_root_location,
      ui::EventTimeForNow(), event.flags(), changed_button_flags);
}

}  // namespace ash
