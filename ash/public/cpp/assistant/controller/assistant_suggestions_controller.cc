// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/controller/assistant_suggestions_controller.h"

#include "base/check_op.h"

namespace ash {

namespace {

AssistantSuggestionsController* g_instance = nullptr;

}  // namespace

AssistantSuggestionsController::AssistantSuggestionsController() {
  DCHECK_EQ(nullptr, g_instance);
  g_instance = this;
}

AssistantSuggestionsController::~AssistantSuggestionsController() {
  DCHECK_EQ(g_instance, this);
  g_instance = nullptr;
}

// static
AssistantSuggestionsController* AssistantSuggestionsController::Get() {
  return g_instance;
}

}  // namespace ash
