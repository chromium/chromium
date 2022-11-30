// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/controller/assistant_interaction_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

AssistantInteractionController* g_instance = nullptr;

}  // namespace

AssistantInteractionController::AssistantInteractionController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AssistantInteractionController::~AssistantInteractionController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AssistantInteractionController* AssistantInteractionController::Get() {
  return g_instance;
}

}  // namespace ash
