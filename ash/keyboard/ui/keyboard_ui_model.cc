// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/keyboard/ui/keyboard_ui_model.h"

#include "base/metrics/histogram_functions.h"

namespace keyboard {

namespace {

// Returns whether a given state transition is valid.
// See the design document linked in https://crbug.com/71990.
bool IsAllowedStateTransition(KeyboardUIState from, KeyboardUIState to) {
  using State = KeyboardUIState;
  switch (GetStateTransitionHash(from, to)) {
    // The initial ShowKeyboard scenario
    // INITIAL -> LOADING -> HIDDEN -> SHOWN.
    case GetStateTransitionHash(State::kInitial, State::kLoading):
    case GetStateTransitionHash(State::kLoading, State::kHidden):
    case GetStateTransitionHash(State::kHidden, State::kShown):

    // Hide scenario
    // SHOWN -> WILL_HIDE -> HIDDEN.
    case GetStateTransitionHash(State::kShown, State::kWillHide):
    case GetStateTransitionHash(State::kWillHide, State::kHidden):

    // Focus transition scenario
    // SHOWN -> WILL_HIDE -> SHOWN.
    case GetStateTransitionHash(State::kWillHide, State::kShown):

    // HideKeyboard can be called at anytime (for example on shutdown).
    case GetStateTransitionHash(State::kShown, State::kHidden):

    // Return to INITIAL when keyboard is disabled.
    case GetStateTransitionHash(State::kLoading, State::kInitial):
    case GetStateTransitionHash(State::kHidden, State::kInitial):
      return true;
    default:
      return false;
  }
}

}  // namespace

std::string StateToStr(KeyboardUIState state) {
  switch (state) {
    case KeyboardUIState::kUnknown:
      return "UNKNOWN";
    case KeyboardUIState::kInitial:
      return "INITIAL";
    case KeyboardUIState::kLoading:
      return "LOADING";
    case KeyboardUIState::kShown:
      return "SHOWN";
    case KeyboardUIState::kWillHide:
      return "WILL_HIDE";
    case KeyboardUIState::kHidden:
      return "HIDDEN";
  }
}

KeyboardUIModel::KeyboardUIModel() = default;

void KeyboardUIModel::ChangeState(KeyboardUIState new_state) {
  DCHECK(IsAllowedStateTransition(state_, new_state))
      << "State: " << StateToStr(state_) << " -> " << StateToStr(new_state)
      << " Unexpected transition";

  if (new_state == state_)
    return;

  state_ = new_state;
}

}  // namespace keyboard
