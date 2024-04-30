// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/kiosk_controller.h"

#include "base/check.h"
#include "base/check_deref.h"

namespace ash {

namespace {

static KioskController* g_instance = nullptr;

}  // namespace

KioskController& KioskController::Get() {
  return CHECK_DEREF(g_instance);
}

KioskController::KioskController() {
  CHECK(!g_instance);
  g_instance = this;
}

KioskController::~KioskController() {
  g_instance = nullptr;
}

}  // namespace ash
