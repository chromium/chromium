// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/quick_answers/controller/quick_answers_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {
QuickAnswersController* g_instance = nullptr;
}

QuickAnswersController::QuickAnswersController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

QuickAnswersController::~QuickAnswersController() {
  DCHECK_EQ(this, g_instance);
  g_instance = nullptr;
}

QuickAnswersController* QuickAnswersController::Get() {
  return g_instance;
}
}  // namespace ash
