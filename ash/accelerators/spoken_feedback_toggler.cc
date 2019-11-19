// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/spoken_feedback_toggler.h"

#include <utility>

#include "ash/accelerators/key_hold_detector.h"
#include "ash/accessibility/accessibility_controller_impl.h"
#include "ash/shell.h"
#include "ui/events/event.h"

namespace ash {
namespace {
bool speech_feedback_toggler_enabled = false;
}

// static
bool SpokenFeedbackToggler::IsEnabled() {
  return speech_feedback_toggler_enabled;
}

// static
void SpokenFeedbackToggler::SetEnabled(bool enabled) {
  speech_feedback_toggler_enabled = enabled;
}

// static
std::unique_ptr<ui::EventHandler> SpokenFeedbackToggler::CreateHandler() {
  std::unique_ptr<KeyHoldDetector::Delegate> delegate(
      new SpokenFeedbackToggler());
  return std::unique_ptr<ui::EventHandler>(
      new KeyHoldDetector(std::move(delegate)));
}

bool SpokenFeedbackToggler::ShouldProcessEvent(
    const ui::KeyEvent* event) const {
  return IsEnabled() && event->key_code() == ui::VKEY_F6;
}

bool SpokenFeedbackToggler::IsStartEvent(const ui::KeyEvent* event) const {
  return event->type() == ui::ET_KEY_PRESSED &&
         event->flags() & ui::EF_SHIFT_DOWN;
}

bool SpokenFeedbackToggler::ShouldStopEventPropagation() const {
  // Let hotkey events pass through. See http://crbug.com/526729
  return false;
}

void SpokenFeedbackToggler::OnKeyHold(const ui::KeyEvent* event) {
  if (!toggled_) {
    toggled_ = true;
    AccessibilityControllerImpl* controller =
        Shell::Get()->accessibility_controller();
    controller->SetSpokenFeedbackEnabled(!controller->spoken_feedback_enabled(),
                                         A11Y_NOTIFICATION_SHOW);
  }
}

void SpokenFeedbackToggler::OnKeyUnhold(const ui::KeyEvent* event) {
  toggled_ = false;
}

SpokenFeedbackToggler::SpokenFeedbackToggler() : toggled_(false) {}

SpokenFeedbackToggler::~SpokenFeedbackToggler() = default;

}  // namespace ash
