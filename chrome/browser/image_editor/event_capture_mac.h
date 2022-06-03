// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_IMAGE_EDITOR_EVENT_CAPTURE_MAC_H_
#define CHROME_BROWSER_IMAGE_EDITOR_EVENT_CAPTURE_MAC_H_

#include "ui/base/cocoa/weak_ptr_nsobject.h"
#include "ui/events/event_handler.h"
#include "ui/gfx/native_widget_types.h"

namespace image_editor {

// A class based on ui/views/event_monitor.h, but specialized for this capture
// mode, allowing for filtering/canceling of events before they reach
// pre-target and post-target handlers.
// For other platforms we attach a pre-target handler to the main WebContents's
// NativeWindow and can catch and consume events there, but some events over
// the main window do not reach that approach on Mac.
class EventCaptureMac {
 public:
  EventCaptureMac(ui::EventHandler* event_handler,
                  gfx::NativeWindow target_window);

  EventCaptureMac(const EventCaptureMac&) = delete;
  EventCaptureMac& operator=(const EventCaptureMac&) = delete;

  ~EventCaptureMac();

  // Allows mouse move events over the affected region requests to set a cross
  // cursor, using a native method.
  static void SetCrossCursor();

 private:
  id monitor_;
  ui::WeakPtrNSObjectFactory<EventCaptureMac> factory_;
};

}  // namespace image_editor

#endif  // CHROME_BROWSER_IMAGE_EDITOR_EVENT_CAPTURE_MAC_H_
