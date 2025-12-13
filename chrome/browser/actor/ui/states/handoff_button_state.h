// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_STATES_HANDOFF_BUTTON_STATE_H_
#define CHROME_BROWSER_ACTOR_UI_STATES_HANDOFF_BUTTON_STATE_H_

#include <ostream>

namespace actor::ui {

// Tab-scoped state.
struct HandoffButtonState {
  enum class ControlOwnership {
    // Represents the state where the client has control over the tab.
    kClient,
    // Represents the state where the actor has control over the tab.
    kActor,
  };

  // Whether or not the component is active.
  // This member is intended to be used alongside the relevant tab's visibility
  // status to determine whether or not the handoff button should be shown.
  bool is_active;
  ControlOwnership controller = ControlOwnership::kClient;

  bool operator==(const HandoffButtonState& other) const = default;
};

inline std::ostream& operator<<(std::ostream& os,
                                const HandoffButtonState& state) {
  os << "HandoffButtonState{"
     << "is_active: " << state.is_active << ", controller: ";
  switch (state.controller) {
    case HandoffButtonState::ControlOwnership::kClient:
      os << "kClient";
      break;
    case HandoffButtonState::ControlOwnership::kActor:
      os << "kActor";
      break;
  }
  os << "}";
  return os;
}
}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_STATES_HANDOFF_BUTTON_STATE_H_
