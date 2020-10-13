// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/caption_buttons/snap_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

SnapController* g_instance = nullptr;

}  // namespace

SnapController::~SnapController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

// static
SnapController* SnapController::Get() {
  return g_instance;
}

SnapController::SnapController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

}  // namespace ash
