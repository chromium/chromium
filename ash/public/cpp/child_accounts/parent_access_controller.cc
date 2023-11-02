// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/child_accounts/parent_access_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {
ParentAccessController* g_instance = nullptr;
}

ParentAccessController::ParentAccessController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

ParentAccessController::~ParentAccessController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

ParentAccessController* ParentAccessController::Get() {
  return g_instance;
}
}  // namespace ash