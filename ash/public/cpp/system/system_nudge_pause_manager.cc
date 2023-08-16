// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/system_nudge_pause_manager.h"

#include "base/check_op.h"

namespace ash {

namespace {

SystemNudgePauseManager* g_instance = nullptr;

}  // namespace

// static
SystemNudgePauseManager* SystemNudgePauseManager::Get() {
  return g_instance;
}

SystemNudgePauseManager::SystemNudgePauseManager() {
  CHECK(!g_instance);
  g_instance = this;
}

SystemNudgePauseManager::~SystemNudgePauseManager() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
