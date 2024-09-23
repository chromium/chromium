// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lobster/lobster_system_state_provider.h"

#include "ash/public/cpp/lobster/lobster_enums.h"
#include "ash/public/cpp/lobster/lobster_system_state.h"

LobsterSystemStateProvider::LobsterSystemStateProvider() = default;

LobsterSystemStateProvider::~LobsterSystemStateProvider() = default;

ash::LobsterSystemState LobsterSystemStateProvider::GetSystemState() {
  return ash::LobsterSystemState(ash::LobsterStatus::kBlocked, {});
}
