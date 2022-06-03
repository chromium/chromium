// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/session/session_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

SessionController* g_instance = nullptr;

}  // namespace

// static
SessionController* SessionController::Get() {
  return g_instance;
}

SessionController::SessionController() {
  DCHECK(!g_instance);
  g_instance = this;
}

SessionController::~SessionController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
