// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_editor/event_capture_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace image_editor {

EventCaptureMac::EventCaptureMac(ui::EventHandler* event_handler,
                                 gfx::NativeWindow target_native_window)
    : factory_(this) {
  DCHECK(event_handler);
  NSWindow* target_window = target_native_window.GetNativeNSWindow();

  // Capture a WeakPtr via NSObject. This allows the block to detect another
  // event monitor for the same event deleting |this|.
  WeakPtrNSObject* handle = factory_.handle();

  auto block = ^NSEvent*(NSEvent* event) {
    if (!ui::WeakPtrNSObjectFactory<EventCaptureMac>::Get(handle))
      return event;

    if (!target_window || [event window] == target_window) {
      std::unique_ptr<ui::Event> ui_event = ui::EventFromNative(event);
      if (!ui_event) {
        return event;
      }
      ui::EventType type = ui_event->type();
      if (type == ui::ET_KEY_PRESSED) {
        event_handler->OnKeyEvent(ui_event->AsKeyEvent());
      }
      if (type == ui::ET_MOUSE_MOVED || type == ui::ET_MOUSE_DRAGGED ||
          type == ui::ET_MOUSE_PRESSED || type == ui::ET_MOUSE_RELEASED) {
        event_handler->OnMouseEvent(ui_event->AsMouseEvent());
      }
      // Consume the event if allowed and the corresponding EventHandler method
      // requested.
      if (ui_event->cancelable() && ui_event->handled()) {
        return nil;
      }
    }
    return event;
  };

  monitor_ = [NSEvent addLocalMonitorForEventsMatchingMask:NSAnyEventMask
                                                   handler:block];
}

EventCaptureMac::~EventCaptureMac() {
  [NSEvent removeMonitor:monitor_];
}

void EventCaptureMac::SetCrossCursor() {
  [[NSCursor crosshairCursor] set];
}

}  // namespace image_editor
