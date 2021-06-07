// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/projector/projector_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {
ProjectorController* g_instance = nullptr;
}

ProjectorController::ProjectorController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

ProjectorController::~ProjectorController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
ProjectorController* ProjectorController::Get() {
  return g_instance;
}

ProjectorController::ScopedInstanceResetterForTest::
    ScopedInstanceResetterForTest()
    : controller_(g_instance) {
  g_instance = nullptr;
}

ProjectorController::ScopedInstanceResetterForTest::
    ~ScopedInstanceResetterForTest() {
  g_instance = controller_;
}

}  // namespace ash
