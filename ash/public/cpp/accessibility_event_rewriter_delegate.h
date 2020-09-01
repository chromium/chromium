// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ACCESSIBILITY_EVENT_REWRITER_DELEGATE_H_
#define ASH_PUBLIC_CPP_ACCESSIBILITY_EVENT_REWRITER_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"

namespace ui {
class Event;
}

namespace ash {

enum class SwitchAccessCommand;

// Allows a client to implement event processing for accessibility features;
// used for ChromeVox and Switch Access.
class ASH_PUBLIC_EXPORT AccessibilityEventRewriterDelegate {
 public:
  // Used to send key events to the ChromeVox extension. |capture| is true if
  // the rewriter discarded the event, false if the rewriter continues event
  // propagation.
  virtual void DispatchKeyEventToChromeVox(std::unique_ptr<ui::Event> event,
                                           bool capture) = 0;

  // Used to send mouse events to the ChromeVox extension.
  virtual void DispatchMouseEventToChromeVox(
      std::unique_ptr<ui::Event> event) = 0;

  // Sends a command to Switch Access.
  virtual void SendSwitchAccessCommand(SwitchAccessCommand command) = 0;

 protected:
  virtual ~AccessibilityEventRewriterDelegate() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ACCESSIBILITY_EVENT_REWRITER_DELEGATE_H_
