// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_DASHBOARD_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_DASHBOARD_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_permission_chip.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_interface.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"

namespace contextual_tasks {

class ContextualTasksLocationBar;

// Aggregator class that implements PermissionDashboardInterface for the side
// panel. Owns both the active permission request chip and the in-use indicator
// chip.
class ContextualTasksPermissionDashboard : public PermissionDashboardInterface {
 public:
  explicit ContextualTasksPermissionDashboard(
      ContextualTasksLocationBar* location_bar);
  ContextualTasksPermissionDashboard(
      const ContextualTasksPermissionDashboard&) = delete;
  ContextualTasksPermissionDashboard& operator=(
      const ContextualTasksPermissionDashboard&) = delete;
  ~ContextualTasksPermissionDashboard() override;

  // PermissionDashboardInterface overrides:
  void SetVisible(bool visible) override;
  bool GetVisible() const override;
  PermissionChipInterface* GetRequestChip() override;
  PermissionChipInterface* GetIndicatorChip() override;
  views::BubbleAnchor GetAnchor() override;

  // Serializes the composite state of both chips into a Mojo struct.
  toolbar_ui_api::mojom::PermissionDashboardStatePtr GetState() const;

  ContextualTasksPermissionChip* request_chip() { return &request_chip_; }
  ContextualTasksPermissionChip* indicator_chip() { return &indicator_chip_; }

 private:
  void UpdateState();

  raw_ptr<ContextualTasksLocationBar> location_bar_;
  bool is_visible_ = false;
  ContextualTasksPermissionChip request_chip_;
  ContextualTasksPermissionChip indicator_chip_;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_DASHBOARD_H_
