// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_STATES_HANDOFF_BUTTON_STATE_H_
#define CHROME_BROWSER_ACTOR_UI_STATES_HANDOFF_BUTTON_STATE_H_

namespace actor::ui {

// Tab-scoped state.
struct HandoffButtonState {
  enum class ControlOwnership {
    // Represents the state where the client has control over the tab.
    kClient,
    // Represents the state where the agent has control over the tab.
    kAgent,
  };

  // Whether or not the component is active.
  // This member is intended to be used alongside the relevant tab's visibility
  // status to determine whether or not the handoff button should be shown.
  bool is_active;
  ControlOwnership controller = ControlOwnership::kClient;

  bool operator==(const HandoffButtonState& other) const = default;
};

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_STATES_HANDOFF_BUTTON_STATE_H_
