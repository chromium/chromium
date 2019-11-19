// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_EVENT_REWRITER_CONTROLLER_IMPL_H_
#define ASH_EVENTS_EVENT_REWRITER_CONTROLLER_IMPL_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/event_rewriter_controller.h"
#include "base/macros.h"
#include "ui/aura/env_observer.h"

namespace ui {
class EventRewriter;
}  // namespace ui

namespace ash {

class KeyboardDrivenEventRewriter;
class SpokenFeedbackEventRewriter;

// Owns ui::EventRewriters and ensures that they are added to each root window
// EventSource, current and future, in the order that they are added to this.
class ASH_EXPORT EventRewriterControllerImpl : public EventRewriterController,
                                               public aura::EnvObserver {
 public:
  EventRewriterControllerImpl();
  ~EventRewriterControllerImpl() override;

  // EventRewriterController:
  void Initialize(ui::EventRewriterChromeOS::Delegate* event_rewriter_delegate,
                  ash::SpokenFeedbackEventRewriterDelegate*
                      spoken_feedback_event_rewriter_delegate) override;
  void AddEventRewriter(std::unique_ptr<ui::EventRewriter> rewriter) override;
  void SetKeyboardDrivenEventRewriterEnabled(bool enabled) override;
  void SetArrowToTabRewritingEnabled(bool enabled) override;
  void OnUnhandledSpokenFeedbackEvent(
      std::unique_ptr<ui::Event> event) override;
  void CaptureAllKeysForSpokenFeedback(bool capture) override;
  void SetSendMouseEventsToDelegate(bool value) override;

  // aura::EnvObserver:
  void OnHostInitialized(aura::WindowTreeHost* host) override;

 private:
  // The |EventRewriter|s managed by this controller.
  std::vector<std::unique_ptr<ui::EventRewriter>> rewriters_;

  // A weak pointer to the KeyboardDrivenEventRewriter owned in |rewriters_|.
  KeyboardDrivenEventRewriter* keyboard_driven_event_rewriter_ = nullptr;

  // A weak pointer to the SpokenFeedbackEventRewriter owned in |rewriters_|.
  SpokenFeedbackEventRewriter* spoken_feedback_event_rewriter_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(EventRewriterControllerImpl);
};

}  // namespace ash

#endif  // ASH_EVENTS_EVENT_REWRITER_CONTROLLER_IMPL_H_
