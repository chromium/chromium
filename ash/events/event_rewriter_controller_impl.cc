// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/event_rewriter_controller_impl.h"

#include <utility>

#include "ash/display/mirror_window_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/events/keyboard_driven_event_rewriter.h"
#include "ash/events/spoken_feedback_event_rewriter.h"
#include "ash/public/cpp/spoken_feedback_event_rewriter_delegate.h"
#include "ash/shell.h"
#include "ash/sticky_keys/sticky_keys_controller.h"
#include "ui/aura/env.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/event_sink.h"
#include "ui/events/event_source.h"

namespace ash {

// static
EventRewriterController* EventRewriterController::Get() {
  return Shell::HasInstance() ? Shell::Get()->event_rewriter_controller()
                              : nullptr;
}

EventRewriterControllerImpl::EventRewriterControllerImpl() {
  // Add the controller as an observer for new root windows.
  aura::Env::GetInstance()->AddObserver(this);
}

EventRewriterControllerImpl::~EventRewriterControllerImpl() {
  aura::Env::GetInstance()->RemoveObserver(this);
  // Remove the rewriters from every root window EventSource and destroy them.
  for (const auto& rewriter : rewriters_) {
    for (auto* window : Shell::GetAllRootWindows())
      window->GetHost()->GetEventSource()->RemoveEventRewriter(rewriter.get());
  }
  rewriters_.clear();
}

void EventRewriterControllerImpl::Initialize(
    ui::EventRewriterChromeOS::Delegate* event_rewriter_delegate,
    ash::SpokenFeedbackEventRewriterDelegate*
        spoken_feedback_event_rewriter_delegate) {
  std::unique_ptr<KeyboardDrivenEventRewriter> keyboard_driven_event_rewriter =
      std::make_unique<KeyboardDrivenEventRewriter>();
  keyboard_driven_event_rewriter_ = keyboard_driven_event_rewriter.get();

  std::unique_ptr<ui::EventRewriterChromeOS> event_rewriter_chromeos =
      std::make_unique<ui::EventRewriterChromeOS>(
          event_rewriter_delegate, ash::Shell::Get()->sticky_keys_controller());

  std::unique_ptr<SpokenFeedbackEventRewriter> spoken_feedback_event_rewriter =
      std::make_unique<SpokenFeedbackEventRewriter>(
          event_rewriter_chromeos.get());
  spoken_feedback_event_rewriter_ = spoken_feedback_event_rewriter.get();
  spoken_feedback_event_rewriter_->set_delegate(
      spoken_feedback_event_rewriter_delegate);

  // EventRewriters are notified in the order they are added.
  AddEventRewriter(std::move(spoken_feedback_event_rewriter));
  AddEventRewriter(std::move(keyboard_driven_event_rewriter));
  AddEventRewriter(std::move(event_rewriter_chromeos));
}

void EventRewriterControllerImpl::AddEventRewriter(
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

void EventRewriterControllerImpl::SetKeyboardDrivenEventRewriterEnabled(
    bool enabled) {
  keyboard_driven_event_rewriter_->set_enabled(enabled);
}

void EventRewriterControllerImpl::SetArrowToTabRewritingEnabled(bool enabled) {
  keyboard_driven_event_rewriter_->set_arrow_to_tab_rewriting_enabled(enabled);
}

void EventRewriterControllerImpl::OnUnhandledSpokenFeedbackEvent(
    std::unique_ptr<ui::Event> event) {
  spoken_feedback_event_rewriter_->OnUnhandledSpokenFeedbackEvent(
      std::move(event));
}

void EventRewriterControllerImpl::CaptureAllKeysForSpokenFeedback(
    bool capture) {
  spoken_feedback_event_rewriter_->set_capture_all_keys(capture);
}

void EventRewriterControllerImpl::SetSendMouseEventsToDelegate(bool value) {
  spoken_feedback_event_rewriter_->set_send_mouse_events(value);
}

void EventRewriterControllerImpl::OnHostInitialized(
    aura::WindowTreeHost* host) {
  for (const auto& rewriter : rewriters_)
    host->GetEventSource()->AddEventRewriter(rewriter.get());
}

}  // namespace ash
