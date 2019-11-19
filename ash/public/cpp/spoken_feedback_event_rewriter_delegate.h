// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_SPOKEN_FEEDBACK_EVENT_REWRITER_DELEGATE_H_
#define ASH_PUBLIC_CPP_SPOKEN_FEEDBACK_EVENT_REWRITER_DELEGATE_H_

#include <memory>

#include "ash/public/cpp/ash_public_export.h"

namespace ui {
class Event;
}

namespace ash {

// Allows a client to implement spoken feedback features; used for ChromeVox.
class ASH_PUBLIC_EXPORT SpokenFeedbackEventRewriterDelegate {
 public:
  // Used to send key events to the ChromeVox extension. |capture| is true if
  // the rewriter discarded the event, false if the rewriter continues event
  // propagation.
  virtual void DispatchKeyEventToChromeVox(std::unique_ptr<ui::Event> event,
                                           bool capture) = 0;

  // Used to send mouse events to the ChromeVox extension.
  virtual void DispatchMouseEventToChromeVox(
      std::unique_ptr<ui::Event> event) = 0;

 protected:
  virtual ~SpokenFeedbackEventRewriterDelegate() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_SPOKEN_FEEDBACK_EVENT_REWRITER_DELEGATE_H_
