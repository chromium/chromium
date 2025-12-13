// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_CLIENT_CONTROLLED_STATE_UTIL_H_
#define ASH_UTILITY_CLIENT_CONTROLLED_STATE_UTIL_H_

#include "ash/ash_export.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "chromeos/ui/base/window_state_type.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {
class WindowState;
class ClientControlledState;

// This is defined as a class to access WindowState's private utility clasess.
class ASH_EXPORT ClientControlledStateUtil {
 public:
  ClientControlledStateUtil() = delete;

  using StateChangeRequestCallback =
      base::RepeatingCallback<void(WindowState* window_state,
                                   ClientControlledState* state,
                                   chromeos::WindowStateType next_state)>;

  using BoundsChangeRequestCallback =
      base::RepeatingCallback<void(WindowState* window_state,
                                   ClientControlledState* state,
                                   chromeos::WindowStateType requested_state,
                                   const gfx::Rect& bounds_in_display,
                                   int64_t display_id)>;

  // Creates and sets the client controlled window state to the window, whose
  // state changes and bounds changes will be applied asynchronosly. Supplied
  // callbacks, which are responsible for applying changes, will be called in a
  // posted tasks, and they can use `ApplyWindowStateRequest` and
  // `ApplyBoundsRequest` method to apply the change. When callbacks are
  // omitted, they're called by default.
  static void BuildAndSet(
      aura::Window* window,
      StateChangeRequestCallback state_change_callback = base::NullCallback(),
      BoundsChangeRequestCallback bounds_change_callback =
          base::NullCallback());

  // Applies the window state change request to the `window_state` and
  // `client_controlled_state`.
  static void ApplyWindowStateRequest(
      WindowState* window_state,
      ClientControlledState* client_controlled_state,
      chromeos::WindowStateType next_state);

  // Applies the bounds state change request to the `window_state` and
  // `client_controlled_state`.
  static void ApplyBoundsRequest(WindowState* window_state,
                                 ClientControlledState* client_controlled_state,
                                 chromeos::WindowStateType requested_state,
                                 const gfx::Rect& bounds_in_display,
                                 int64_t display_id);
};

}  // namespace ash

#endif  // ASH_UTILITY_CLIENT_CONTROLLED_STATE_UTIL_H_
