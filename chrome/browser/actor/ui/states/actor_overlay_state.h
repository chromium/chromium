// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_UI_STATES_ACTOR_OVERLAY_STATE_H_
#define CHROME_BROWSER_ACTOR_UI_STATES_ACTOR_OVERLAY_STATE_H_

#include <optional>

#include "chrome/browser/actor/shared_types.h"

namespace actor::ui {

// Tab-scoped state.
struct ActorOverlayState {
  // Whether or not the component is active.
  // This member is intended to be used alongside the relevant tab's visibility
  // status to determine whether or not the actor overlay should be shown.
  bool is_active;
  // A magic mouse click was triggered.
  bool mouse_down;
  // The target at which the magic mouse should be over.
  std::optional<PageTarget> mouse_target = std::nullopt;

  explicit ActorOverlayState(
      bool is_active = false,
      bool mouse_down = false,
      std::optional<PageTarget> mouse_target = std::nullopt);
  ~ActorOverlayState();
  ActorOverlayState(const ActorOverlayState&);
  bool operator==(const ActorOverlayState& other) const;
};

inline std::ostream& operator<<(std::ostream& os,
                                const ActorOverlayState& state) {
  os << "ActorOverlayState{"
     << "is_active: " << state.is_active << ", mouse_down: " << state.mouse_down
     << ", mouse_target: ";
  if (state.mouse_target.has_value()) {
    os << state.mouse_target.value();
  } else {
    os << "nullopt";
  }
  os << "}";
  return os;
}

}  // namespace actor::ui

#endif  // CHROME_BROWSER_ACTOR_UI_STATES_ACTOR_OVERLAY_STATE_H_
