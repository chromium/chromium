// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_SPOKEN_FEEDBACK_EVENT_REWRITER_H_
#define ASH_EVENTS_SPOKEN_FEEDBACK_EVENT_REWRITER_H_

#include "ash/ash_export.h"
#include "ash/public/interfaces/event_rewriter_controller.mojom.h"
#include "base/macros.h"
#include "ui/events/event_rewriter.h"

namespace ash {

// SpokenFeedbackEventRewriter sends key events to ChromeVox (via the delegate)
// when spoken feedback is enabled. Continues dispatch of unhandled key events.
// TODO(http://crbug.com/839541): Avoid reposting unhandled events.
class ASH_EXPORT SpokenFeedbackEventRewriter : public ui::EventRewriter {
 public:
  SpokenFeedbackEventRewriter();
  ~SpokenFeedbackEventRewriter() override;

  // Set the delegate used to send key events to the ChromeVox extension.
  void SetDelegate(mojom::SpokenFeedbackEventRewriterDelegatePtr delegate);
  mojom::SpokenFeedbackEventRewriterDelegatePtr* get_delegate_for_testing() {
    return &delegate_;
  }

  // Continue dispatch of events that were unhandled by the ChromeVox extension.
  // NOTE: These events may be delivered out-of-order from non-ChromeVox events.
  void OnUnhandledSpokenFeedbackEvent(std::unique_ptr<ui::Event> event) const;

  void set_capture_all_keys(bool value) { capture_all_keys_ = value; }
  void set_send_mouse_events(bool value) { send_mouse_events_ = value; }

 private:
  // ui::EventRewriter:
  ui::EventRewriteStatus RewriteEvent(
      const ui::Event& event,
      std::unique_ptr<ui::Event>* new_event) override;
  ui::EventRewriteStatus NextDispatchEvent(
      const ui::Event& last_event,
      std::unique_ptr<ui::Event>* new_event) override;

  // The delegate used to send key events to the ChromeVox extension.
  mojom::SpokenFeedbackEventRewriterDelegatePtr delegate_;

  // Whether to send mouse events to the ChromeVox extension.
  bool send_mouse_events_ = false;

  // Whether to capture all keys.
  bool capture_all_keys_ = false;

  DISALLOW_COPY_AND_ASSIGN(SpokenFeedbackEventRewriter);
};

}  // namespace ash

#endif  // ASH_EVENTS_SPOKEN_FEEDBACK_EVENT_REWRITER_H_
