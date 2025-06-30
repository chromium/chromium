// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_STATES_AGENT_OVERLAY_STATE_H_
#define CHROME_BROWSER_ACTOR_UI_STATES_AGENT_OVERLAY_STATE_H_

namespace actor {

// Tab-scoped state.
struct AgentOverlayState {
  // Whether or not the component is active.
  bool is_active;
  // TODO(crbug.com/424495020): Add support for coordinate/dom location state

  bool operator==(const AgentOverlayState& other) const = default;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_UI_STATES_AGENT_OVERLAY_STATE_H_
