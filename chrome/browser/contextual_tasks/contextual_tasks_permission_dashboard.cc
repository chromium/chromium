// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_permission_dashboard.h"

#include "base/functional/bind.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_location_bar.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_permission_chip.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"

namespace contextual_tasks {

ContextualTasksPermissionDashboard::ContextualTasksPermissionDashboard(
    ContextualTasksLocationBar* location_bar)
    : location_bar_(location_bar),
      request_chip_(
          location_bar,
          PermissionChipView::kPermissionRequestChipElementId,
          base::BindRepeating(&ContextualTasksPermissionDashboard::UpdateState,
                              base::Unretained(this))),
      indicator_chip_(
          location_bar,
          PermissionChipView::kIndicatorChipElementId,
          base::BindRepeating(&ContextualTasksPermissionDashboard::UpdateState,
                              base::Unretained(this))) {}

ContextualTasksPermissionDashboard::~ContextualTasksPermissionDashboard() =
    default;

void ContextualTasksPermissionDashboard::SetVisible(bool visible) {
  if (is_visible_ == visible) {
    return;
  }
  is_visible_ = visible;
  UpdateState();
}

bool ContextualTasksPermissionDashboard::GetVisible() const {
  return is_visible_;
}

PermissionChipInterface* ContextualTasksPermissionDashboard::GetRequestChip() {
  return &request_chip_;
}

PermissionChipInterface*
ContextualTasksPermissionDashboard::GetIndicatorChip() {
  return &indicator_chip_;
}

views::BubbleAnchor ContextualTasksPermissionDashboard::GetAnchor() {
  if (request_chip_.GetVisible()) {
    return request_chip_.GetAnchor();
  }
  if (indicator_chip_.GetVisible()) {
    return indicator_chip_.GetAnchor();
  }
  return views::BubbleAnchor();
}

toolbar_ui_api::mojom::PermissionDashboardStatePtr
ContextualTasksPermissionDashboard::GetState() const {
  auto state = toolbar_ui_api::mojom::PermissionDashboardState::New();
  state->request_chip = request_chip_.GetState();
  state->indicator_chip = indicator_chip_.GetState();

  // `PermissionDashboardState` has no visibility field of its own, and its chip
  // fields are non-nullable, so a hidden dashboard is expressed as two hidden
  // chips rather than a null state.
  if (!is_visible_) {
    state->request_chip->is_visible = false;
    state->indicator_chip->is_visible = false;
  }

  state->is_divider_visible =
      is_visible_ && request_chip_.GetVisible() && indicator_chip_.GetVisible();
  return state;
}

void ContextualTasksPermissionDashboard::UpdateState() {
  if (location_bar_) {
    location_bar_->OnChanged();
  }
}

}  // namespace contextual_tasks
