// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/system/power/power_button_controller_base.h"

#include "base/check_op.h"

namespace ash {

namespace {

PowerButtonControllerBase* g_instance = nullptr;

}  // namespace

// static
PowerButtonControllerBase* PowerButtonControllerBase::Get() {
  return g_instance;
}

PowerButtonControllerBase::PowerButtonControllerBase() {
  DCHECK(!g_instance);
  g_instance = this;
}

PowerButtonControllerBase::~PowerButtonControllerBase() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

}  // namespace ash
