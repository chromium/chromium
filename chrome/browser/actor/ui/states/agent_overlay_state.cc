// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/ui/states/agent_overlay_state.h"

namespace actor::ui {

AgentOverlayState::AgentOverlayState(bool is_active,
                                     bool mouse_down,
                                     std::optional<PageTarget> mouse_target)
    : is_active(is_active),
      mouse_down(mouse_down),
      mouse_target(mouse_target) {}
AgentOverlayState::AgentOverlayState(const AgentOverlayState&) = default;
AgentOverlayState::~AgentOverlayState() = default;

bool AgentOverlayState::operator==(const AgentOverlayState& other) const =
    default;

}  // namespace actor::ui
