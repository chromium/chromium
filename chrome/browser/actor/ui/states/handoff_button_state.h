// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_STATES_HANDOFF_BUTTON_STATE_H_
#define CHROME_BROWSER_ACTOR_UI_STATES_HANDOFF_BUTTON_STATE_H_

namespace actor {

// Tab-scoped state.
struct HandoffButtonState {
  enum class ControlOwnership {
    // Represents the state where the client has control over the tab.
    kClient,
    // Represents the state where the agent has control over the tab.
    kAgent,
  };
  // Whether or not the component is active.
  bool is_active;
  ControlOwnership controller;

  bool operator==(const HandoffButtonState& other) const = default;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_UI_STATES_HANDOFF_BUTTON_STATE_H_
