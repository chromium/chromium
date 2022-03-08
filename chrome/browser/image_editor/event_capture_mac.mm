// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/image_editor/event_capture_mac.h"

#import <Cocoa/Cocoa.h>

#include "base/callback.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"

namespace image_editor {

EventCaptureMac::EventCaptureMac(ui::EventHandler* event_handler,
                                 base::OnceClosure capture_lost_callback,
                                 gfx::NativeView web_contents_view,
                                 gfx::NativeWindow target_native_window)
    : capture_lost_callback_(std::move(capture_lost_callback)), factory_(this) {
  event_handler_ = event_handler;
  web_contents_view_ = web_contents_view.GetNativeNSView();
  window_ = target_native_window.GetNativeNSWindow();
  mouse_capture_ = std::make_unique<remote_cocoa::CocoaMouseCapture>(this);
}

EventCaptureMac::~EventCaptureMac() {
  // We do not want our callback to run if mouse capture loss was caused by
  // reset of event capture.
  std::move(capture_lost_callback_).Reset();
  mouse_capture_.reset();
}

bool EventCaptureMac::PostCapturedEvent(NSEvent* event) {
  std::unique_ptr<ui::Event> ui_event = ui::EventFromNative(event);
  if (!ui_event)
    return false;

  // The window from where the event is sourced. If it is outside of the
  // browser, this window will not be equal to GetWindow().
  NSWindow* source = [event window];
  NSView* contentView = [source contentView];
  NSView* view = [contentView hitTest:[event locationInWindow]];

  ui::EventType type = ui_event->type();
  if (type == ui::ET_MOUSE_DRAGGED || type == ui::ET_MOUSE_RELEASED) {
    event_handler_->OnMouseEvent(ui_event->AsMouseEvent());
  } else if ((type == ui::ET_MOUSE_PRESSED || type == ui::ET_MOUSE_MOVED) &&
             web_contents_view_ == view) {
    // We do not need to record mouse clicks outside of the web contents.
    event_handler_->OnMouseEvent(ui_event->AsMouseEvent());
  } else if (type == ui::ET_SCROLL) {
    event_handler_->OnScrollEvent(ui_event->AsScrollEvent());
  }

  // If we set the ui event as handled, then we want to swallow the event.
  return ui_event->handled();
}

void EventCaptureMac::OnMouseCaptureLost() {
  if (!capture_lost_callback_.is_null())
    std::move(capture_lost_callback_).Run();
}

NSWindow* EventCaptureMac::GetWindow() const {
  return window_;
}

void EventCaptureMac::SetCrossCursor() {
  [[NSCursor crosshairCursor] set];
}

}  // namespace image_editor
