// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/event_rewriter_controller.h"

#include <utility>

#include "ash/display/mirror_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/events/keyboard_driven_event_rewriter.h"
#include "ash/events/spoken_feedback_event_rewriter.h"
#include "ash/shell.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_rewriter.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_source.h"

namespace ash {

EventRewriterController::EventRewriterController() {
  // Add the controller as an observer for new root windows.
  Shell::Get()->aura_env()->AddObserver(this);

  std::unique_ptr<KeyboardDrivenEventRewriter> keyboard_driven_event_rewriter =
      std::make_unique<KeyboardDrivenEventRewriter>();
  keyboard_driven_event_rewriter_ = keyboard_driven_event_rewriter.get();
  AddEventRewriter(std::move(keyboard_driven_event_rewriter));

  std::unique_ptr<SpokenFeedbackEventRewriter> spoken_feedback_event_rewriter =
      std::make_unique<SpokenFeedbackEventRewriter>();
  spoken_feedback_event_rewriter_ = spoken_feedback_event_rewriter.get();
  AddEventRewriter(std::move(spoken_feedback_event_rewriter));
}

EventRewriterController::~EventRewriterController() {
  Shell::Get()->aura_env()->RemoveObserver(this);
  // Remove the rewriters from every root window EventSource and destroy them.
  for (const auto& rewriter : rewriters_) {
    for (auto* window : Shell::GetAllRootWindows())
      window->GetHost()->GetEventSource()->RemoveEventRewriter(rewriter.get());
  }
  rewriters_.clear();
}

void EventRewriterController::AddEventRewriter(
    std::unique_ptr<ui::EventRewriter> rewriter) {
  // Add the rewriters to each existing root window EventSource.
  for (auto* window : Shell::GetAllRootWindows())
    window->GetHost()->GetEventSource()->AddEventRewriter(rewriter.get());

  // In case there are any mirroring displays, their hosts' EventSources won't
  // be included above.
  const auto* mirror_window_controller =
      Shell::Get()->window_tree_host_manager()->mirror_window_controller();
  for (auto* window : mirror_window_controller->GetAllRootWindows())
    window->GetHost()->GetEventSource()->AddEventRewriter(rewriter.get());

  rewriters_.push_back(std::move(rewriter));
}

void EventRewriterController::BindRequest(
    mojom::EventRewriterControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void EventRewriterController::SetKeyboardDrivenEventRewriterEnabled(
    bool enabled) {
  keyboard_driven_event_rewriter_->set_enabled(enabled);
}

void EventRewriterController::SetArrowToTabRewritingEnabled(bool enabled) {
  keyboard_driven_event_rewriter_->set_arrow_to_tab_rewriting_enabled(enabled);
}

void EventRewriterController::SetSpokenFeedbackEventRewriterDelegate(
    mojom::SpokenFeedbackEventRewriterDelegatePtr delegate) {
  spoken_feedback_event_rewriter_->SetDelegate(std::move(delegate));
}

void EventRewriterController::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) {
  spoken_feedback_event_rewriter_->OnUnhandledSpokenFeedbackEvent(
      std::move(event));
}

void EventRewriterController::CaptureAllKeysForSpokenFeedback(bool capture) {
  spoken_feedback_event_rewriter_->set_capture_all_keys(capture);
}

void EventRewriterController::SetSendMouseEventsToDelegate(bool value) {
  spoken_feedback_event_rewriter_->set_send_mouse_events(value);
}

void EventRewriterController::OnHostInitialized(aura::WindowTreeHost* host) {
  for (const auto& rewriter : rewriters_)
    host->GetEventSource()->AddEventRewriter(rewriter.get());
}

}  // namespace ash
