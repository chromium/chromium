// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shutdown_controller.h"

#include "base/logging.h"

namespace ash {

namespace {

ShutdownController* g_instance = nullptr;

}  // namespace

ShutdownController::ScopedResetterForTest::ScopedResetterForTest()
    : instance_(g_instance) {
  g_instance = nullptr;
}

ShutdownController::ScopedResetterForTest::~ScopedResetterForTest() {
  g_instance = instance_;
}

// static
ShutdownController* ShutdownController::Get() {
  return g_instance;
}

ShutdownController::ShutdownController() {
  DCHECK(!g_instance);
  g_instance = this;
}

ShutdownController::~ShutdownController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
