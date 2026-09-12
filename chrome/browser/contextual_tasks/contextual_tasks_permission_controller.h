// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CONTROLLER_H_

#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom-forward.h"

namespace content {
class WebContents;
}

namespace contextual_tasks {

// Implements the toolbar mojom interface to control
// the dashboard and permission chip for contextual tasks.
class ContextualTasksPermissionController {
 public:
  virtual ~ContextualTasksPermissionController() = default;

  static ContextualTasksPermissionController* FromWebContents(
      content::WebContents* web_contents) {
    return nullptr;
  }

  virtual toolbar_ui_api::mojom::PermissionDashboardStatePtr GetState() = 0;
  virtual void OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier,
      bool is_mouse_interaction) = 0;
  virtual void OnChipExpandAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) = 0;
  virtual void OnChipCollapseAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier identifier) = 0;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CONTROLLER_H_
