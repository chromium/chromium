// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_SPOKEN_FEEDBACK_EVENT_REWRITER_H_
#define ASH_EVENTS_SPOKEN_FEEDBACK_EVENT_REWRITER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/events/event_rewriter.h"

namespace ui {
class EventRewriterChromeOS;
}

namespace ash {

class SpokenFeedbackEventRewriterDelegate;

// SpokenFeedbackEventRewriter sends key events to ChromeVox (via the delegate)
// when spoken feedback is enabled. Continues dispatch of unhandled key events.
// TODO(http://crbug.com/839541): Avoid reposting unhandled events.
class ASH_EXPORT SpokenFeedbackEventRewriter : public ui::EventRewriter {
 public:
  explicit SpokenFeedbackEventRewriter(
      ui::EventRewriterChromeOS* event_rewriter_chromeos);
  ~SpokenFeedbackEventRewriter() override;

  // Set the delegate used to send key events to the ChromeVox extension.
  void set_delegate(SpokenFeedbackEventRewriterDelegate* delegate) {
    delegate_ = delegate;
  }

  // Continue dispatch of events that were unhandled by the ChromeVox extension.
  // NOTE: These events may be delivered out-of-order from non-ChromeVox events.
  void OnUnhandledSpokenFeedbackEvent(std::unique_ptr<ui::Event> event) const;

  void set_capture_all_keys(bool value) { capture_all_keys_ = value; }
  void set_send_mouse_events(bool value) { send_mouse_events_ = value; }

 private:
  // ui::EventRewriter:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  // Continuation saved for OnUnhandledSpokenFeedbackEvent().
  Continuation continuation_;

  // The delegate used to send key events to the ChromeVox extension.
  SpokenFeedbackEventRewriterDelegate* delegate_ = nullptr;

  // Whether to send mouse events to the ChromeVox extension.
  bool send_mouse_events_ = false;

  // Whether to capture all keys.
  bool capture_all_keys_ = false;

  // Weak.
  ui::EventRewriterChromeOS* event_rewriter_chromeos_;

  DISALLOW_COPY_AND_ASSIGN(SpokenFeedbackEventRewriter);
};

}  // namespace ash

#endif  // ASH_EVENTS_SPOKEN_FEEDBACK_EVENT_REWRITER_H_
