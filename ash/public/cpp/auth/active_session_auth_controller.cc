// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/auth/active_session_auth_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {
ActiveSessionAuthController* g_instance = nullptr;
}

ActiveSessionAuthController::ActiveSessionAuthController() {
  CHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

ActiveSessionAuthController::~ActiveSessionAuthController() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

ActiveSessionAuthController* ActiveSessionAuthController::Get() {
  return g_instance;
}
}  // namespace ash
