// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_editor/event_capture_mac.h"

#import <Cocoa/Cocoa.h>

#include <memory>

#include "base/apple/owned_objc.h"
#include "base/check.h"
#include "base/functional/callback.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#import "components/remote_cocoa/app_shim/mouse_capture.h"
#import "components/remote_cocoa/app_shim/mouse_capture_delegate.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace image_editor {

class EventCaptureMac::MouseCaptureDelegateImpl
    : public remote_cocoa::CocoaMouseCaptureDelegate {
 public:
  MouseCaptureDelegateImpl(ui::EventHandler* event_handler,
                           base::OnceClosure capture_lost_callback,
                           gfx::NativeView web_contents_view,
                           gfx::NativeWindow target_native_window)
      : event_handler_(event_handler),
        capture_lost_callback_(std::move(capture_lost_callback)),
        web_contents_view_(web_contents_view.GetNativeNSView()),
        window_(target_native_window.GetNativeNSWindow()),
        mouse_capture_(
            std::make_unique<remote_cocoa::CocoaMouseCapture>(this)) {}

  void SetKeyboardMonitor(id local_keyboard_monitor) {
    local_keyboard_monitor_ = local_keyboard_monitor;
  }

  void Reset() {
    // We do not want our callback to run if mouse capture loss was caused by
    // reset of event capture.
    std::move(capture_lost_callback_).Reset();

    // Remove the key down monitor.
    [NSEvent removeMonitor:local_keyboard_monitor_];
  }

 private:
  // remote_cocoa::CocoaMouseCaptureDelegate:
  bool PostCapturedEvent(NSEvent* event) override {
    std::unique_ptr<ui::Event> ui_event =
        ui::EventFromNative(base::apple::OwnedNSEvent(event));
    if (!ui_event) {
      return false;
    }

    // The window from where the event is sourced. If it is outside of the
    // browser, this window will not be equal to GetWindow().
    NSView* view = [event.window.contentView hitTest:event.locationInWindow];

    ui::EventType type = ui_event->type();
    if (type == ui::EventType::kMouseDragged ||
        type == ui::EventType::kMouseReleased) {
      event_handler_->OnMouseEvent(ui_event->AsMouseEvent());
    } else if ((type == ui::EventType::kMousePressed ||
                type == ui::EventType::kMouseMoved) &&
               web_contents_view_ == view) {
      // We do not need to record mouse clicks outside of the web contents.
      event_handler_->OnMouseEvent(ui_event->AsMouseEvent());
    } else if (type == ui::EventType::kMouseMoved &&
               web_contents_view_ != view) {
      // Manually set arrow cursor when region search UI is open and cursor is
      // moved from web contents.
      [NSCursor.arrowCursor set];
    } else if (type == ui::EventType::kScroll) {
      event_handler_->OnScrollEvent(ui_event->AsScrollEvent());
    }

    // If we set the ui event as handled, then we want to swallow the event.
    return ui_event->handled();
  }

  void OnMouseCaptureLost() override {
    if (!capture_lost_callback_.is_null()) {
      std::move(capture_lost_callback_).Run();
    }
  }

  NSWindow* GetWindow() const override { return window_; }

  raw_ptr<ui::EventHandler> event_handler_;
  base::OnceClosure capture_lost_callback_;
  NSView* __weak web_contents_view_ = nil;
  NSWindow* __weak window_ = nil;
  std::unique_ptr<remote_cocoa::CocoaMouseCapture> mouse_capture_;
  id __strong local_keyboard_monitor_ = nil;
};

EventCaptureMac::EventCaptureMac(ui::EventHandler* event_handler,
                                 base::OnceClosure capture_lost_callback,
                                 gfx::NativeView web_contents_view,
                                 gfx::NativeWindow target_native_window)
    : mouse_capture_delegate_impl_(std::make_unique<MouseCaptureDelegateImpl>(
          event_handler,
          std::move(capture_lost_callback),
          web_contents_view,
          target_native_window)) {
  CreateKeyDownLocalMonitor(event_handler, target_native_window);
}

void EventCaptureMac::CreateKeyDownLocalMonitor(
    ui::EventHandler* event_handler,
    gfx::NativeWindow target_native_window) {
  DCHECK(event_handler);
  NSWindow* target_window = target_native_window.GetNativeNSWindow();

  // Capture a WeakPtr. This allows the block to detect another event monitor
  // for the same event deleting |this|.
  base::WeakPtr<EventCaptureMac> weak_ptr = factory_.GetWeakPtr();

  auto block = ^NSEvent*(NSEvent* event) {
    if (!weak_ptr) {
      return event;
    }

    if (!target_window || event.window == target_window) {
      std::unique_ptr<ui::Event> ui_event =
          ui::EventFromNative(base::apple::OwnedNSEvent(event));
      if (!ui_event) {
        return event;
      }
      ui::EventType type = ui_event->type();
      if (type == ui::EventType::kKeyPressed) {
        event_handler->OnKeyEvent(ui_event->AsKeyEvent());
      }
      // Consume the event if allowed and the corresponding EventHandler method
      // requested.
      if (ui_event->cancelable() && ui_event->handled()) {
        return nil;
      }
    }
    return event;
  };

  mouse_capture_delegate_impl_->SetKeyboardMonitor([NSEvent
      addLocalMonitorForEventsMatchingMask:NSEventMaskKeyDown
                                   handler:block]);
}

EventCaptureMac::~EventCaptureMac() {
  mouse_capture_delegate_impl_->Reset();
}

void EventCaptureMac::SetCrossCursor() {
  [NSCursor.crosshairCursor set];
}

}  // namespace image_editor
