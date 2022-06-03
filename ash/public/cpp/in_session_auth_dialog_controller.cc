// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/in_session_auth_dialog_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {
InSessionAuthDialogController* g_instance = nullptr;
}

// static
InSessionAuthDialogController* InSessionAuthDialogController::Get() {
  return g_instance;
}

InSessionAuthDialogController::InSessionAuthDialogController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

InSessionAuthDialogController::~InSessionAuthDialogController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
