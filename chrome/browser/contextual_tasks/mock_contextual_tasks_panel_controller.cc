// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/mock_contextual_tasks_panel_controller.h"

#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"

namespace contextual_tasks {

MockContextualTasksPanelController::MockContextualTasksPanelController() =
    default;

MockContextualTasksPanelController::MockContextualTasksPanelController(
    BrowserWindowInterface* browser_window) {
  if (browser_window) {
    scoped_unowned_user_data_.emplace(browser_window->GetUnownedUserDataHost(),
                                      *this);
  }
}

MockContextualTasksPanelController::~MockContextualTasksPanelController() =
    default;

}  // namespace contextual_tasks
