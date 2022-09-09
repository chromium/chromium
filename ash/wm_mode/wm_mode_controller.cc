// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm_mode/wm_mode_controller.h"

#include "base/check.h"
#include "base/check_op.h"

namespace ash {

namespace {

WmModeController* g_instance = nullptr;

}  // namespace

WmModeController::WmModeController() {
  DCHECK(!g_instance);
  g_instance = this;
}

WmModeController::~WmModeController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
WmModeController* WmModeController::Get() {
  DCHECK(g_instance);
  return g_instance;
}

void WmModeController::Toggle() {
  is_active_ = !is_active_;
}

}  // namespace ash
