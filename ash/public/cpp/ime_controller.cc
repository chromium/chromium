// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ime_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

ImeController* g_instance = nullptr;

}  // namespace

// static
void ImeController::SetInstanceForTest(ImeController* instance) {
  g_instance = instance;
}

// static
ImeController* ImeController::Get() {
  return g_instance;
}

ImeController::~ImeController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

ImeController::ImeController() {
  DCHECK(!g_instance);
  g_instance = this;
}

}  // namespace ash
