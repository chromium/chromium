// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/lock_screen_profile_creator.h"

#include <memory>
#include <utility>

#include "base/check_op.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"

namespace lock_screen_apps {

LockScreenProfileCreator::LockScreenProfileCreator() {}

LockScreenProfileCreator::~LockScreenProfileCreator() {}

void LockScreenProfileCreator::Initialize() {
  CHECK_EQ(state_, State::kNotInitialized);
  state_ = State::kInitialized;

  InitializeImpl();
}

void LockScreenProfileCreator::AddCreateProfileCallback(
    base::OnceClosure callback) {
  CHECK_NE(state_, State::kNotInitialized);

  if (ProfileCreated()) {
    std::move(callback).Run();
    return;
  }

  create_profile_callbacks_.emplace_back(std::move(callback));
}

bool LockScreenProfileCreator::Initialized() const {
  return state_ != State::kNotInitialized;
}

bool LockScreenProfileCreator::ProfileCreated() const {
  return state_ == State::kProfileCreated;
}

void LockScreenProfileCreator::OnLockScreenProfileCreateStarted() {
  CHECK_EQ(State::kInitialized, state_);

  state_ = State::kCreatingProfile;
}

void LockScreenProfileCreator::OnLockScreenProfileCreated(
    Profile* lock_screen_profile) {
  CHECK_EQ(State::kCreatingProfile, state_);
  state_ = State::kProfileCreated;

  lock_screen_profile_ = lock_screen_profile;

  while (!create_profile_callbacks_.empty()) {
    std::move(create_profile_callbacks_.front()).Run();
    create_profile_callbacks_.pop_front();
  }
}

}  // namespace lock_screen_apps
