// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/browser_delegate/browser_controller.h"

#include "base/check.h"
#include "base/check_op.h"

namespace {
ash::BrowserController* g_browser_controller = nullptr;
}  // namespace

namespace ash {

BrowserController* BrowserController::GetInstance() {
  return g_browser_controller;
}

BrowserController::BrowserController() {
  CHECK(!g_browser_controller);
  g_browser_controller = this;
}

BrowserController::~BrowserController() {
  CHECK_EQ(g_browser_controller, this);
  g_browser_controller = nullptr;
}

}  // namespace ash
