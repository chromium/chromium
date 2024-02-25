// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login/local_authentication_request_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {
LocalAuthenticationRequestController* g_instance = nullptr;
}

LocalAuthenticationRequestController::LocalAuthenticationRequestController() {
  CHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

LocalAuthenticationRequestController::~LocalAuthenticationRequestController() {
  CHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

LocalAuthenticationRequestController*
LocalAuthenticationRequestController::Get() {
  return g_instance;
}
}  // namespace ash
