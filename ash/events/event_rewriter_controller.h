// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_EVENTS_EVENT_REWRITER_CONTROLLER_H_
#define ASH_EVENTS_EVENT_REWRITER_CONTROLLER_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/interfaces/event_rewriter_controller.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "ui/aura/env_observer.h"

namespace ui {
class EventRewriter;
class EventSource;
}  // namespace ui

namespace ash {

class KeyboardDrivenEventRewriter;
class SpokenFeedbackEventRewriter;

// Owns ui::EventRewriters and ensures that they are added to each root window
// EventSource, current and future, in the order that they are added to this.
// TODO(crbug.com/647781): Avoid Chrome's direct access in Classic Ash mode.
class ASH_EXPORT EventRewriterController
    : public mojom::EventRewriterController,
      public aura::EnvObserver {
 public:
  EventRewriterController();
  ~EventRewriterController() override;

  // Takes ownership of |rewriter| and adds it to the current event sources.
  void AddEventRewriter(std::unique_ptr<ui::EventRewriter> rewriter);

  // Binds the mojom::EventRewriterController interface request to this object.
  void BindRequest(mojom::EventRewriterControllerRequest request);

  // mojom::EventRewriterController:
  void SetKeyboardDrivenEventRewriterEnabled(bool enabled) override;
  void SetArrowToTabRewritingEnabled(bool enabled) override;
  void SetSpokenFeedbackEventRewriterDelegate(
      mojom::SpokenFeedbackEventRewriterDelegatePtr delegate) override;
  void OnUnhandledSpokenFeedbackEvent(
      std::unique_ptr<ui::Event> event) override;
  void CaptureAllKeysForSpokenFeedback(bool capture) override;
  void SetSendMouseEventsToDelegate(bool value) override;

  // aura::EnvObserver:
  void OnWindowInitialized(aura::Window* window) override {}
  void OnHostInitialized(aura::WindowTreeHost* host) override;

 private:
  // The |EventRewriter|s managed by this controller.
  std::vector<std::unique_ptr<ui::EventRewriter>> rewriters_;

  // A weak pointer to the KeyboardDrivenEventRewriter owned in |rewriters_|.
  KeyboardDrivenEventRewriter* keyboard_driven_event_rewriter_;

  // A weak pointer to the SpokenFeedbackEventRewriter owned in |rewriters_|.
  SpokenFeedbackEventRewriter* spoken_feedback_event_rewriter_;

  // Bindings for the EventRewriterController mojo interface.
  mojo::BindingSet<mojom::EventRewriterController> bindings_;

  DISALLOW_COPY_AND_ASSIGN(EventRewriterController);
};

}  // namespace ash

#endif  // ASH_EVENTS_EVENT_REWRITER_CONTROLLER_H_
