// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/events/event_rewriter_controller_impl.h"

#include <utility>

#include "ash/accessibility/sticky_keys/sticky_keys_controller.h"
#include "ash/constants/ash_features.h"
#include "ash/display/mirror_window_controller.h"
#include "ash/display/privacy_screen_controller.h"
#include "ash/display/window_tree_host_manager.h"
#include "ash/events/accessibility_event_rewriter.h"
#include "ash/events/keyboard_driven_event_rewriter.h"
#include "ash/events/peripheral_customization_event_rewriter.h"
#include "ash/public/cpp/accessibility_event_rewriter_delegate.h"
#include "ash/shell.h"
#include "base/command_line.h"
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
  // Remove the rewriters from every root window EventSource.
  for (auto* window : Shell::GetAllRootWindows()) {
    auto* event_source = window->GetHost()->GetEventSource();
    for (const auto& rewriter : rewriters_) {
      event_source->RemoveEventRewriter(rewriter.get());
    }
  }
}

void EventRewriterControllerImpl::Initialize(
    ui::EventRewriterAsh::Delegate* event_rewriter_delegate,
    AccessibilityEventRewriterDelegate* accessibility_event_rewriter_delegate) {
  std::unique_ptr<KeyboardDrivenEventRewriter> keyboard_driven_event_rewriter =
      std::make_unique<KeyboardDrivenEventRewriter>();
  keyboard_driven_event_rewriter_ = keyboard_driven_event_rewriter.get();

  bool privacy_screen_supported = false;
  if (Shell::Get()->privacy_screen_controller() &&
      Shell::Get()->privacy_screen_controller()->IsSupported()) {
    privacy_screen_supported = true;
  }

  event_rewriter_ash_delegate_ = event_rewriter_delegate;
  std::unique_ptr<ui::EventRewriterAsh> event_rewriter_ash =
      std::make_unique<ui::EventRewriterAsh>(
          event_rewriter_delegate, Shell::Get()->keyboard_capability(),
          Shell::Get()->sticky_keys_controller(), privacy_screen_supported);
  event_rewriter_ash_ = event_rewriter_ash.get();

  std::unique_ptr<PeripheralCustomizationEventRewriter>
      peripheral_customization_event_rewriter;
  if (features::IsPeripheralCustomizationEnabled()) {
    peripheral_customization_event_rewriter =
        std::make_unique<PeripheralCustomizationEventRewriter>();
    peripheral_customization_event_rewriter_ =
        peripheral_customization_event_rewriter.get();
  }

  std::unique_ptr<AccessibilityEventRewriter> accessibility_event_rewriter =
      std::make_unique<AccessibilityEventRewriter>(
          event_rewriter_ash.get(), accessibility_event_rewriter_delegate);
  accessibility_event_rewriter_ = accessibility_event_rewriter.get();

  // EventRewriters are notified in the order they are added.
  AddEventRewriter(std::move(accessibility_event_rewriter));
  if (features::IsPeripheralCustomizationEnabled()) {
    AddEventRewriter(std::move(peripheral_customization_event_rewriter));
  }
  AddEventRewriter(std::move(keyboard_driven_event_rewriter));
  AddEventRewriter(std::move(event_rewriter_ash));
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
  accessibility_event_rewriter_->OnUnhandledSpokenFeedbackEvent(
      std::move(event));
}

void EventRewriterControllerImpl::CaptureAllKeysForSpokenFeedback(
    bool capture) {
  accessibility_event_rewriter_->set_chromevox_capture_all_keys(capture);
}

void EventRewriterControllerImpl::SetSendMouseEvents(bool value) {
  accessibility_event_rewriter_->set_send_mouse_events(value);
}

void EventRewriterControllerImpl::SetAltDownRemappingEnabled(bool enabled) {
  if (event_rewriter_ash_) {
    event_rewriter_ash_->set_alt_down_remapping_enabled(enabled);
  }
}

void EventRewriterControllerImpl::OnHostInitialized(
    aura::WindowTreeHost* host) {
  for (const auto& rewriter : rewriters_)
    host->GetEventSource()->AddEventRewriter(rewriter.get());
}

}  // namespace ash
