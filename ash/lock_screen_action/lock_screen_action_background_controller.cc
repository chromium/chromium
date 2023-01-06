// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lock_screen_action/lock_screen_action_background_controller.h"

#include "ash/lock_screen_action/lock_screen_action_background_controller_impl.h"
#include "ash/lock_screen_action/lock_screen_action_background_controller_stub.h"
#include "ash/lock_screen_action/lock_screen_action_background_observer.h"
#include "base/functional/callback.h"

namespace ash {

namespace {

LockScreenActionBackgroundController::FactoryCallback*
    g_testing_factory_callback = nullptr;

}  // namespace

// static
std::unique_ptr<LockScreenActionBackgroundController>
LockScreenActionBackgroundController::Create() {
  if (g_testing_factory_callback)
    return g_testing_factory_callback->Run();
  return std::make_unique<LockScreenActionBackgroundControllerImpl>();
}

// static
void LockScreenActionBackgroundController::SetFactoryCallbackForTesting(
    FactoryCallback* testing_factory_callback) {
  g_testing_factory_callback = testing_factory_callback;
}

LockScreenActionBackgroundController::LockScreenActionBackgroundController() =
    default;

LockScreenActionBackgroundController::~LockScreenActionBackgroundController() {
  UpdateState(LockScreenActionBackgroundState::kHidden);
}

void LockScreenActionBackgroundController::SetParentWindow(
    aura::Window* parent_window) {
  DCHECK(!parent_window_);
  parent_window_ = parent_window;
}

void LockScreenActionBackgroundController::AddObserver(
    LockScreenActionBackgroundObserver* observer) {
  observers_.AddObserver(observer);
}

void LockScreenActionBackgroundController::RemoveObserver(
    LockScreenActionBackgroundObserver* observer) {
  observers_.RemoveObserver(observer);
}

void LockScreenActionBackgroundController::UpdateState(
    LockScreenActionBackgroundState state) {
  if (state_ == state)
    return;

  state_ = state;

  for (auto& observer : observers_)
    observer.OnLockScreenActionBackgroundStateChanged(state_);
}

}  // namespace ash
