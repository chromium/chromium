// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/keyboard/keyboard_controller.h"

#include "base/check_op.h"

namespace ash {

// static
KeyboardController* KeyboardController::Get() {
  return g_instance_;
}

KeyboardController::KeyboardController() {
  DCHECK(!g_instance_);
  g_instance_ = this;
}

KeyboardController::~KeyboardController() {
  DCHECK_EQ(g_instance_, this);
  g_instance_ = nullptr;
}

// static
KeyboardController* KeyboardController::g_instance_ = nullptr;

}  // namespace ash
