// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/magnifier_key_scroller.h"

#include <utility>

#include "ash/accelerators/key_hold_detector.h"
#include "ash/keyboard/keyboard_util.h"
#include "ash/magnifier/magnification_controller.h"
#include "ash/public/cpp/ash_switches.h"
#include "ash/shell.h"
#include "base/command_line.h"
#include "ui/events/event.h"

namespace ash {
namespace {
bool magnifier_key_scroller_enabled = false;
}

// static
bool MagnifierKeyScroller::IsEnabled() {
  bool has_switch = base::CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kAshEnableMagnifierKeyScroller);

  return (magnifier_key_scroller_enabled || has_switch) &&
         ash::Shell::Get()->magnification_controller()->IsEnabled();
}

// static
void MagnifierKeyScroller::SetEnabled(bool enabled) {
  magnifier_key_scroller_enabled = enabled;
}

// static
std::unique_ptr<ui::EventHandler> MagnifierKeyScroller::CreateHandler() {
  std::unique_ptr<KeyHoldDetector::Delegate> delegate(
      new MagnifierKeyScroller());
  return std::unique_ptr<ui::EventHandler>(
      new KeyHoldDetector(std::move(delegate)));
}

bool MagnifierKeyScroller::ShouldProcessEvent(const ui::KeyEvent* event) const {
  return IsEnabled() && ash::keyboard_util::IsArrowKeyCode(event->key_code());
}

bool MagnifierKeyScroller::IsStartEvent(const ui::KeyEvent* event) const {
  return event->type() == ui::ET_KEY_PRESSED &&
         event->flags() & ui::EF_SHIFT_DOWN;
}

bool MagnifierKeyScroller::ShouldStopEventPropagation() const {
  return true;
}

void MagnifierKeyScroller::OnKeyHold(const ui::KeyEvent* event) {
  MagnificationController* controller =
      Shell::Get()->magnification_controller();
  switch (event->key_code()) {
    case ui::VKEY_UP:
      controller->SetScrollDirection(MagnificationController::SCROLL_UP);
      break;
    case ui::VKEY_DOWN:
      controller->SetScrollDirection(MagnificationController::SCROLL_DOWN);
      break;
    case ui::VKEY_LEFT:
      controller->SetScrollDirection(MagnificationController::SCROLL_LEFT);
      break;
    case ui::VKEY_RIGHT:
      controller->SetScrollDirection(MagnificationController::SCROLL_RIGHT);
      break;
    default:
      NOTREACHED() << "Unknown keyboard_code:" << event->key_code();
  }
}

void MagnifierKeyScroller::OnKeyUnhold(const ui::KeyEvent* event) {
  MagnificationController* controller =
      Shell::Get()->magnification_controller();
  controller->SetScrollDirection(MagnificationController::SCROLL_NONE);
}

MagnifierKeyScroller::MagnifierKeyScroller() = default;

MagnifierKeyScroller::~MagnifierKeyScroller() = default;

}  // namespace ash
