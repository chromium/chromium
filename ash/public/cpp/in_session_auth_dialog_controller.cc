// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/in_session_auth_dialog_controller.h"

namespace ash {

namespace {

// non owning pointer, the singleton's lifetime is instead managed by ash::Shell
InSessionAuthDialogController* g_instance = nullptr;

}  // namespace

// static
InSessionAuthDialogController* InSessionAuthDialogController::Get() {
  return g_instance;
}

InSessionAuthDialogController::InSessionAuthDialogController() {
  DCHECK(!g_instance);
  g_instance = this;
}

InSessionAuthDialogController::~InSessionAuthDialogController() {
  DCHECK(g_instance);
  g_instance = nullptr;
}

}  // namespace ash
