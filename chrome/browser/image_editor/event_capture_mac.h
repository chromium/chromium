// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_EDITOR_EVENT_CAPTURE_MAC_H_
#define CHROME_BROWSER_IMAGE_EDITOR_EVENT_CAPTURE_MAC_H_

#include "base/callback.h"
#include "components/remote_cocoa/app_shim/mouse_capture.h"
#include "components/remote_cocoa/app_shim/mouse_capture_delegate.h"
#include "ui/base/cocoa/weak_ptr_nsobject.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace image_editor {

// A class to capture mouse events on Mac and forward them to an event handler.
// For other platforms we attach a pre-target handler to the main WebContents's
// NativeWindow and can catch and consume events there, but some events over
// the main window do not reach that approach on Mac.
class EventCaptureMac : public remote_cocoa::CocoaMouseCaptureDelegate {
 public:
  EventCaptureMac(ui::EventHandler* event_handler,
                  base::OnceClosure capture_lost_callback,
                  gfx::NativeView web_contents_view,
                  gfx::NativeWindow target_window);
  ~EventCaptureMac() override;
  EventCaptureMac(const EventCaptureMac&) = delete;
  EventCaptureMac& operator=(const EventCaptureMac&) = delete;

  // Allows mouse move events over the affected region requests to set a cross
  // cursor, using a native method.
  static void SetCrossCursor();

  // remote_cocoa::CocoaMouseCaptureDelegate
  bool PostCapturedEvent(NSEvent* event) override;
  void OnMouseCaptureLost() override;
  NSWindow* GetWindow() const override;

 private:
  base::OnceClosure capture_lost_callback_;
  NSView* web_contents_view_;
  NSWindow* window_;
  ui::EventHandler* event_handler_;
  ui::WeakPtrNSObjectFactory<EventCaptureMac> factory_;
  std::unique_ptr<remote_cocoa::CocoaMouseCapture> mouse_capture_;
};

}  // namespace image_editor

#endif  // CHROME_BROWSER_IMAGE_EDITOR_EVENT_CAPTURE_MAC_H_
