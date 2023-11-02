// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_
#define ASH_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_

#include "ash/ash_export.h"
#include "ui/events/event_rewriter.h"

namespace ash {

// KeyboardDrivenEventRewriter removes the modifier flags from
// Shift+<Arrow keys|Enter|F6> key events. This mapping only happens
// on login screen and only when the keyboard driven oobe is enabled.
class ASH_EXPORT KeyboardDrivenEventRewriter : public ui::EventRewriter {
 public:
  KeyboardDrivenEventRewriter();

  KeyboardDrivenEventRewriter(const KeyboardDrivenEventRewriter&) = delete;
  KeyboardDrivenEventRewriter& operator=(const KeyboardDrivenEventRewriter&) =
      delete;

  ~KeyboardDrivenEventRewriter() override;

  // Calls Rewrite for testing.
  ui::EventDispatchDetails RewriteForTesting(const ui::Event& event,
                                             const Continuation continuation);

  // EventRewriter overrides:
  ui::EventDispatchDetails RewriteEvent(
      const ui::Event& event,
      const Continuation continuation) override;

  void set_enabled(bool enabled) { enabled_ = enabled; }
  void set_arrow_to_tab_rewriting_enabled(bool enabled) {
    arrow_to_tab_rewriting_enabled_ = enabled;
  }

 private:
  ui::EventDispatchDetails Rewrite(const ui::Event& event,
                                   const Continuation continuation);

  // If true, this rewriter is enabled. It is only active before user login.
  bool enabled_ = false;

  // If true, Shift + Arrow keys are rewritten to Tab/Shift-Tab keys.
  // This only applies when the KeyboardDrivenEventRewriter is active.
  bool arrow_to_tab_rewriting_enabled_ = false;
};

}  // namespace ash

#endif  // ASH_EVENTS_KEYBOARD_DRIVEN_EVENT_REWRITER_H_
