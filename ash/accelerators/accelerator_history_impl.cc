// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/accelerators/accelerator_history_impl.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "ui/events/event.h"
#include "ui/events/event_target.h"

namespace ash {

namespace {

bool ShouldFilter(ui::KeyEvent* event) {
  const ui::EventType type = event->type();
  if (!event->target() ||
      (type != ui::EventType::kKeyPressed &&
       type != ui::EventType::kKeyReleased) ||
      event->is_char() || !event->target() ||
      // Key events with key code of VKEY_PROCESSKEY, usually created by virtual
      // keyboard (like handwriting input), have no effect on accelerator and
      // they may disturb the accelerator history. So filter them out. (see
      // https://crbug.com/918317)
      event->key_code() == ui::VKEY_PROCESSKEY) {
    return true;
  }

  return false;
}

}  // namespace

AcceleratorHistoryImpl::AcceleratorHistoryImpl() = default;

AcceleratorHistoryImpl::~AcceleratorHistoryImpl() = default;

void AcceleratorHistoryImpl::OnKeyEvent(ui::KeyEvent* event) {
  DCHECK(event->target());
  if (!ShouldFilter(event))
    StoreCurrentAccelerator(ui::Accelerator(*event));
}

void AcceleratorHistoryImpl::OnMouseEvent(ui::MouseEvent* event) {
  if (event->type() == ui::EventType::kMousePressed ||
      event->type() == ui::EventType::kMouseReleased) {
    InterruptCurrentAccelerator();
  }
}

void AcceleratorHistoryImpl::StoreCurrentAccelerator(
    const ui::Accelerator& accelerator) {
  // Track the currently pressed keys so that we don't mistakenly store an
  // already pressed key as a new keypress after another key has been released.
  // As an example, when the user presses and holds Alt+Search, then releases
  // Alt but keeps holding the Search key down, at this point no new Search
  // presses should be stored in the history after the Alt release, since Search
  // was never released in the first place. crbug.com/704280.
  if (accelerator.key_state() == ui::Accelerator::KeyState::PRESSED) {
    if (!currently_pressed_keys_.emplace(accelerator.key_code()).second)
      return;
  } else {
    if (!currently_pressed_keys_.erase(accelerator.key_code()) &&
        (!last_logged_key_code_.has_value() ||
         *last_logged_key_code_ != accelerator.key_code())) {
      // Save the key code to prevent spammy logs.
      last_logged_key_code_ = accelerator.key_code();
      // If the released accelerator doesn't have a corresponding press stored,
      // likely the language was changed between press and release. Clear
      // `currently_pressed_keys_` to prevent keys being left pressed.
      std::string pressed_keys;
      for (auto key_code : currently_pressed_keys_)
        pressed_keys.append(base::NumberToString(key_code).append(" "));

      // Key release was delivered with no corresponding press. This usually
      // happens when the key press is lost somehow.
      currently_pressed_keys_.clear();
    }
  }

  if (accelerator != current_accelerator_) {
    previous_accelerator_ = current_accelerator_;
    current_accelerator_ = accelerator;
  }
}

void AcceleratorHistoryImpl::InterruptCurrentAccelerator() {
  if (current_accelerator_.key_state() == ui::Accelerator::KeyState::PRESSED) {
    // Only interrupts pressed keys.
    current_accelerator_.set_interrupted_by_mouse_event(true);
  }
}

}  // namespace ash
