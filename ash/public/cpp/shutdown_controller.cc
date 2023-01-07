// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/shutdown_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

ShutdownController* g_instance = nullptr;

}  // namespace

template <>
ShutdownController*&
ShutdownController::ScopedResetterForTest::GetGlobalInstanceHolder() {
  return g_instance;
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
