// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_permission_controller.h"

#include <memory>
#include <utility>

#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

#if !BUILDFLAG(IS_ANDROID)
#include "chrome/browser/contextual_tasks/contextual_tasks_location_bar.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_permission_chip.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_permission_dashboard.h"
#include "chrome/browser/ui/location_bar/location_bar_override_data.h"
#endif

namespace contextual_tasks {

WEB_CONTENTS_USER_DATA_KEY_IMPL(ContextualTasksPermissionController);

ContextualTasksPermissionController::ContextualTasksPermissionController(
    content::WebContents* web_contents,
    BrowserWindowInterface* browser_window)
    : content::WebContentsUserData<ContextualTasksPermissionController>(
          *web_contents) {
#if !BUILDFLAG(IS_ANDROID)
  if (browser_window) {
    // Connect dashboard and location bar for contextual tasks side panel.
    location_bar_ =
        std::make_unique<ContextualTasksLocationBar>(browser_window);

    // Register the location bar as the override for this WebContents.
    location_bar::LocationBarOverrideData::CreateForWebContents(
        &GetWebContents(), location_bar_.get());
  }
#endif

  // Observe PermissionRequestManager on this tab for permission prompt
  // visibility updates.
  if (auto* prm = permissions::PermissionRequestManager::FromWebContents(
          &GetWebContents())) {
    prm_observation_.Observe(prm);
  }
}

ContextualTasksPermissionController::~ContextualTasksPermissionController() =
    default;

toolbar_ui_api::mojom::PermissionDashboardStatePtr
ContextualTasksPermissionController::GetState() const {
#if !BUILDFLAG(IS_ANDROID)
  if (location_bar_ && location_bar_->permission_dashboard()) {
    return location_bar_->permission_dashboard()->GetState();
  }
#endif
  auto state = toolbar_ui_api::mojom::PermissionDashboardState::New();
  state->indicator_chip = toolbar_ui_api::mojom::PermissionChipState::New();
  state->request_chip = toolbar_ui_api::mojom::PermissionChipState::New();
  return state;
}

void ContextualTasksPermissionController::PushStateToWebUI() {}

// ============================================================================
// permissions::PermissionRequestManager::Observer:
// ============================================================================

void ContextualTasksPermissionController::OnRequestsFinalized() {
  PushStateToWebUI();
}

void ContextualTasksPermissionController::OnPromptRemoved() {
  PushStateToWebUI();
}

void ContextualTasksPermissionController::OnNavigation(
    content::NavigationHandle* navigation_handle) {
  if (navigation_handle && navigation_handle->IsInPrimaryMainFrame()) {
    PushStateToWebUI();
  }
}

void ContextualTasksPermissionController::
    OnPermissionRequestManagerDestructed() {
  prm_observation_.Reset();
}

// ============================================================================
// Chip Interactions (called from Mojo via ContextualTasksUI):
// ============================================================================

#if !BUILDFLAG(IS_ANDROID)
ContextualTasksPermissionChip* ContextualTasksPermissionController::GetChip(
    toolbar_ui_api::mojom::LhsChipIdentifier chip_identifier) {
  ContextualTasksPermissionDashboard* permission_dashboard =
      location_bar_ ? location_bar_->permission_dashboard() : nullptr;
  if (!permission_dashboard) {
    return nullptr;
  }
  switch (chip_identifier) {
    case toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionRequest:
      return permission_dashboard->request_chip();
    case toolbar_ui_api::mojom::LhsChipIdentifier::kPermissionIndicator:
      return permission_dashboard->indicator_chip();
    default:
      NOTREACHED();
  }
}
#endif

void ContextualTasksPermissionController::OnChipClicked(
    toolbar_ui_api::mojom::LhsChipIdentifier chip_identifier,
    bool is_mouse_interaction) {
#if !BUILDFLAG(IS_ANDROID)
  if (ContextualTasksPermissionChip* chip = GetChip(chip_identifier)) {
    chip->OnClicked(is_mouse_interaction);
  }

  // Ignore the event if the chip does not exist (if dashboard is torn down
  // and mojo event arrives afterwards).
#endif
}

void ContextualTasksPermissionController::OnChipExpandAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier chip_identifier) {
#if !BUILDFLAG(IS_ANDROID)
  if (ContextualTasksPermissionChip* chip = GetChip(chip_identifier)) {
    chip->OnExpandAnimationEnded();
  }
#endif
}

void ContextualTasksPermissionController::OnChipCollapseAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier chip_identifier) {
#if !BUILDFLAG(IS_ANDROID)
  if (ContextualTasksPermissionChip* chip = GetChip(chip_identifier)) {
    chip->OnCollapseAnimationEnded();
  }
#endif
}

}  // namespace contextual_tasks
