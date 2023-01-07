// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/style/dark_light_mode_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

DarkLightModeController* g_instance = nullptr;

}  // namespace

// static
DarkLightModeController* DarkLightModeController::Get() {
  return g_instance;
}

DarkLightModeController::DarkLightModeController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

DarkLightModeController::~DarkLightModeController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
