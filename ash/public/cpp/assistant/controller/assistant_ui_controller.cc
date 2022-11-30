// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/controller/assistant_ui_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

AssistantUiController* g_instance = nullptr;

}  // namespace

AssistantUiController::AssistantUiController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AssistantUiController::~AssistantUiController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AssistantUiController* AssistantUiController::Get() {
  return g_instance;
}

}  // namespace ash
