// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/fjord_oobe/fjord_oobe_state_manager.h"

#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/ash/login/fjord_oobe/fjord_oobe_util.h"
#include "chrome/browser/ash/login/fjord_oobe/proto/fjord_oobe_state.pb.h"

namespace ash {
namespace {
FjordOobeStateManager* g_state_manager = nullptr;
}

// static
void FjordOobeStateManager::Initialize() {
  CHECK(!g_state_manager);
  g_state_manager = new FjordOobeStateManager();
}

// static
void FjordOobeStateManager::Shutdown() {
  CHECK(g_state_manager);
  delete g_state_manager;
  g_state_manager = nullptr;
}

// static
FjordOobeStateManager* FjordOobeStateManager::Get() {
  return g_state_manager;
}

FjordOobeStateManager::FjordOobeStateManager() {
  current_state_ = fjord_util::ShouldShowFjordOobe()
                       ? fjord_oobe_state::proto::FjordOobeStateInfo::
                             FJORD_OOBE_STATE_UNSPECIFIED
                       : fjord_oobe_state::proto::FjordOobeStateInfo::
                             FJORD_OOBE_STATE_UNIMPLEMENTED;
}

FjordOobeStateManager::~FjordOobeStateManager() = default;

fjord_oobe_state::proto::FjordOobeStateInfo
FjordOobeStateManager::GetFjordOobeStateInfo() {
  fjord_oobe_state::proto::FjordOobeStateInfo message;
  message.set_oobe_state(current_state_);
  return message;
}

void FjordOobeStateManager::SetFjordOobeState(
    fjord_oobe_state::proto::FjordOobeStateInfo::FjordOobeState new_state) {
  if (!fjord_util::ShouldShowFjordOobe()) {
    LOG(ERROR) << "Cannot set OOBE state when feature is not enabled";
    return;
  }
  VLOG(1) << "Setting OOBE state to: " << new_state;
  current_state_ = new_state;
  fjord_oobe_state::proto::FjordOobeStateInfo state;
  state.set_oobe_state(current_state_);
  for (auto& observer : observers_) {
    observer.OnFjordOobeStateChanged(state);
  }
}

void FjordOobeStateManager::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void FjordOobeStateManager::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}
}  // namespace ash
