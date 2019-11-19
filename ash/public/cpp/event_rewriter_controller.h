// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_EVENT_REWRITER_CONTROLLER_H_
#define ASH_PUBLIC_CPP_EVENT_REWRITER_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"

namespace ui {
class Event;
class EventRewriter;
}  // namespace ui

namespace ash {

class SpokenFeedbackEventRewriterDelegate;

// Allows clients to toggle some event rewriting behavior.
class ASH_EXPORT EventRewriterController {
 public:
  // Returns the singleton EventRewriterController instance.
  static EventRewriterController* Get();

  // Initializes this controller after ash::Shell finishes initialization.
  virtual void Initialize(
      ui::EventRewriterChromeOS::Delegate* event_rewriter_delegate,
      ash::SpokenFeedbackEventRewriterDelegate*
          spoken_feedback_event_rewriter_delegate) = 0;

  // Takes ownership of |rewriter| and adds it to the current event sources.
  virtual void AddEventRewriter(
      std::unique_ptr<ui::EventRewriter> rewriter) = 0;

  // Enables the KeyboardDrivenEventRewriter, which is disabled by default.
  // This only applies when the user is on the login screen.
  virtual void SetKeyboardDrivenEventRewriterEnabled(bool enabled) = 0;

  // If true, Shift + Arrow keys are rewritten to Tab/Shift-Tab keys.
  // This only applies when the KeyboardDrivenEventRewriter is active.
  virtual void SetArrowToTabRewritingEnabled(bool enabled) = 0;

  // Continue dispatch of key events that were unhandled by ChromeVox.
  // TODO(crbug.com/839541): ChromeVox should not repost unhandled events.
  virtual void OnUnhandledSpokenFeedbackEvent(
      std::unique_ptr<ui::Event> event) = 0;

  // Discards key events and sends to spoken feedback when true.
  virtual void CaptureAllKeysForSpokenFeedback(bool capture) = 0;

  // Sends mouse events to ChromeVox when true.
  virtual void SetSendMouseEventsToDelegate(bool value) = 0;

 protected:
  virtual ~EventRewriterController() {}
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_EVENT_REWRITER_CONTROLLER_H_
