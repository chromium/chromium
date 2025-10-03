// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/utility/client_controlled_state_util.h"

#include "ash/wm/client_controlled_state.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "chromeos/ui/base/window_properties.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/base/mojom/window_show_state.mojom.h"
#include "ui/display/screen.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {
namespace {

// A WindowStateDelegate that implements ToggleFullscreen behavior for
// client controlled window.
class ClientControlledWindowStateDelegate : public WindowStateDelegate {
 public:
  explicit ClientControlledWindowStateDelegate(
      ClientControlledState::Delegate* delegate)
      : delegate_(delegate) {}

  ClientControlledWindowStateDelegate(
      const ClientControlledWindowStateDelegate&) = delete;
  ClientControlledWindowStateDelegate& operator=(
      const ClientControlledWindowStateDelegate&) = delete;

  ~ClientControlledWindowStateDelegate() override = default;

  // WindowStateDelegate:
  bool ToggleFullscreen(WindowState* window_state) override {
    chromeos::WindowStateType next_state;
    aura::Window* window = window_state->window();
    switch (window_state->GetStateType()) {
      case chromeos::WindowStateType::kDefault:
      case chromeos::WindowStateType::kNormal:
        next_state = chromeos::WindowStateType::kFullscreen;
        break;
      case chromeos::WindowStateType::kMaximized:
        next_state = chromeos::WindowStateType::kFullscreen;
        break;
      case chromeos::WindowStateType::kFullscreen:
        switch (window->GetProperty(aura::client::kRestoreShowStateKey)) {
          case ui::mojom::WindowShowState::kDefault:
          case ui::mojom::WindowShowState::kNormal:
            next_state = chromeos::WindowStateType::kNormal;
            break;
          case ui::mojom::WindowShowState::kMaximized:
            next_state = chromeos::WindowStateType::kMaximized;
            break;
          case ui::mojom::WindowShowState::kMinimized:
            next_state = chromeos::WindowStateType::kMinimized;
            break;
          case ui::mojom::WindowShowState::kFullscreen:
          case ui::mojom::WindowShowState::kInactive:
          case ui::mojom::WindowShowState::kEnd:
            DUMP_WILL_BE_NOTREACHED()
                << " unknown state :"
                << window->GetProperty(aura::client::kRestoreShowStateKey);
            return false;
        }
        break;
      case chromeos::WindowStateType::kMinimized: {
        next_state = chromeos::WindowStateType::kFullscreen;
        break;
      }
      default:
        return false;
    }
    delegate_->HandleWindowStateRequest(window_state, next_state);
    return true;
  }

  void OnWindowDestroying() override { delegate_ = nullptr; }

 private:
  raw_ptr<ClientControlledState::Delegate> delegate_;
};

// A ClientControlledStateDelegate that applies the state/bounds asynchronously.
class ClientControlledStateDelegate : public ClientControlledState::Delegate {
 public:
  ClientControlledStateDelegate(
      ClientControlledStateUtil::StateChangeRequestCallback
          state_change_callback,
      ClientControlledStateUtil::BoundsChangeRequestCallback
          bounds_change_callback)
      : state_change_callback_(state_change_callback),
        bounds_change_callback_(bounds_change_callback) {
    if (state_change_callback_.is_null()) {
      state_change_callback_ = base::BindRepeating(
          &ClientControlledStateUtil::ApplyWindowStateRequest);
    }
    if (bounds_change_callback.is_null()) {
      bounds_change_callback_ =
          base::BindRepeating(&ClientControlledStateUtil::ApplyBoundsRequest);
    }
  }

  ClientControlledStateDelegate(const ClientControlledStateDelegate&) = delete;
  ClientControlledStateDelegate& operator=(
      const ClientControlledStateDelegate&) = delete;

  ~ClientControlledStateDelegate() override = default;

  // Overridden from ClientControlledState::Delegate:
  void HandleWindowStateRequest(WindowState* window_state,
                                chromeos::WindowStateType next_state) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(state_change_callback_, base::Unretained(window_state),
                       base::Unretained(client_controlled_state_), next_state));
  }

  void HandleBoundsRequest(WindowState* window_state,
                           chromeos::WindowStateType requested_state,
                           const gfx::Rect& bounds_in_display,
                           int64_t display_id) override {
    base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
        FROM_HERE,
        base::BindOnce(bounds_change_callback_, base::Unretained(window_state),
                       base::Unretained(client_controlled_state_),
                       requested_state, bounds_in_display, display_id));
  }

  void set_client_controlled_state(ClientControlledState* state) {
    client_controlled_state_ = state;
  }

  raw_ptr<ClientControlledState> client_controlled_state_;
  ClientControlledStateUtil::StateChangeRequestCallback state_change_callback_;
  ClientControlledStateUtil::BoundsChangeRequestCallback
      bounds_change_callback_;

  base::WeakPtrFactory<ClientControlledStateDelegate> weak_ptr_factory_{this};
};

}  // namespace

// static
void ClientControlledStateUtil::ApplyWindowStateRequest(
    WindowState* window_state,
    ClientControlledState* client_controlled_state,
    chromeos::WindowStateType next_state) {
  std::optional<gfx::Rect> bounds;
  auto display =
      display::Screen::Get()->GetDisplayNearestWindow(window_state->window());

  switch (next_state) {
    case chromeos::WindowStateType::kDefault:
    case chromeos::WindowStateType::kNormal:
      bounds = window_state->GetRestoreBoundsInScreen();
      break;
    case chromeos::WindowStateType::kMaximized:
      window_state->SaveCurrentBoundsForRestore();
      bounds = display.work_area();
      break;
    case chromeos::WindowStateType::kFullscreen:
      window_state->SaveCurrentBoundsForRestore();
      bounds = display.bounds();
      break;
    case chromeos::WindowStateType::kMinimized:
      break;

    case chromeos::WindowStateType::kPrimarySnapped:
      window_state->SaveCurrentBoundsForRestore();
      bounds = display.bounds();
      bounds->set_width(bounds->width() / 2);
      break;
    case chromeos::WindowStateType::kSecondarySnapped:
      window_state->SaveCurrentBoundsForRestore();
      bounds = display.bounds();
      bounds->set_x(bounds->width() / 2);
      break;
    case chromeos::WindowStateType::kFloated:
      // Additional ash states.
      return;
    case chromeos::WindowStateType::kInactive:
    case chromeos::WindowStateType::kPinned:
    case chromeos::WindowStateType::kLockedFullscreen:
    case chromeos::WindowStateType::kPip:
      // Not supported;
      return;
  }

  client_controlled_state->EnterNextState(window_state, next_state);
  if (bounds) {
    gfx::Rect bounds_in_parent = *bounds;
    wm::ConvertRectFromScreen(window_state->window()->parent(),
                              &bounds_in_parent);
    window_state->SetBoundsDirectCrossFade(bounds_in_parent, true);
  }
}

// static
void ClientControlledStateUtil::ApplyBoundsRequest(
    WindowState* window_state,
    ClientControlledState* client_controlled_state,
    chromeos::WindowStateType requested_state,
    const gfx::Rect& bounds_in_display,
    int64_t display_id) {
  display::Display target_display;
  CHECK(display::Screen::Get()->GetDisplayWithDisplayId(display_id,
                                                        &target_display));
  auto target_bounds = bounds_in_display;
  target_bounds.Offset(target_display.bounds().origin().x(),
                       target_display.bounds().origin().y());

  if (client_controlled_state->EnterNextState(window_state, requested_state)) {
    client_controlled_state->set_next_bounds_change_animation_type(
        WindowState::BoundsChangeAnimationType::kCrossFadeFloat);
  }
  client_controlled_state->set_bounds_locally(true);
  window_state->window()->SetBoundsInScreen(target_bounds, target_display);
  client_controlled_state->set_bounds_locally(false);
}

// static
void ClientControlledStateUtil::BuildAndSet(
    aura::Window* window,
    StateChangeRequestCallback state_change_callback,
    BoundsChangeRequestCallback bounds_change_callback) {
  auto delegate = std::make_unique<ClientControlledStateDelegate>(
      state_change_callback, bounds_change_callback);
  auto* delegate_ptr = delegate.get();
  auto state = std::make_unique<ClientControlledState>(std::move(delegate));
  delegate_ptr->set_client_controlled_state(state.get());

  auto window_state_delegate =
      std::make_unique<ClientControlledWindowStateDelegate>(delegate.get());
  auto* window_state = WindowState::Get(window);
  window_state->SetStateObject(std::move(state));
  window_state->SetDelegate(std::move(window_state_delegate));
  window_state->set_allow_set_bounds_direct(true);
}

}  // namespace ash
