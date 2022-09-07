// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/assistant/assistant_setup.h"

#include "base/check_op.h"

namespace ash {
namespace {
AssistantSetup* g_assistant_setup = nullptr;
}

// static
AssistantSetup* AssistantSetup::GetInstance() {
  return g_assistant_setup;
}

AssistantSetup::AssistantSetup() {
  DCHECK(!g_assistant_setup);
  g_assistant_setup = this;
}

AssistantSetup::~AssistantSetup() {
  DCHECK_EQ(g_assistant_setup, this);
  g_assistant_setup = nullptr;
}

}  // namespace ash
