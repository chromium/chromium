// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_panel_controller.h"

#include "chrome/browser/contextual_tasks/contextual_tasks_panel_host.h"

namespace contextual_tasks {

bool ContextualTasksPanelController::IsOnboardingTooltipVisible() const {
  return false;
}

void ContextualTasksPanelHost::SetErrorPageVisible(bool visible) {}

bool ContextualTasksPanelHost::IsErrorPageVisible() const {
  return false;
}

}  // namespace contextual_tasks
