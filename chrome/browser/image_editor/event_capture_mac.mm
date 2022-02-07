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
  event_handler_ = event_handler;
  window_ = target_native_window.GetNativeNSWindow();
  mouse_capture_ = std::make_unique<remote_cocoa::CocoaMouseCapture>(this);
}

EventCaptureMac::~EventCaptureMac() = default;

void EventCaptureMac::PostCapturedEvent(NSEvent* event) {
  std::unique_ptr<ui::Event> ui_event = ui::EventFromNative(event);
  if (!ui_event)
    return;

  ui::EventType type = ui_event->type();
  if (type == ui::ET_MOUSE_MOVED || type == ui::ET_MOUSE_DRAGGED ||
      type == ui::ET_MOUSE_PRESSED || type == ui::ET_MOUSE_RELEASED) {
    event_handler_->OnMouseEvent(ui_event->AsMouseEvent());
  } else if (type == ui::ET_SCROLL) {
    event_handler_->OnScrollEvent(ui_event->AsScrollEvent());
  }
}

void EventCaptureMac::OnMouseCaptureLost() {
  mouse_capture_.reset();
}

NSWindow* EventCaptureMac::GetWindow() const {
  return window_;
}

void EventCaptureMac::SetCrossCursor() {
  [[NSCursor crosshairCursor] set];
}

}  // namespace image_editor
