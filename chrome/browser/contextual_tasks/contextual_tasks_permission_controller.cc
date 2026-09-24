// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_permission_controller.h"

#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_ui_base.h"
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

ContextualTasksPermissionController::ContextualTasksPermissionController(
    BrowserWindowInterface* browser_window) {
#if !BUILDFLAG(IS_ANDROID)
  if (browser_window) {
    // Connect dashboard and single location bar for contextual tasks side
    // panel.
    location_bar_ = std::make_unique<ContextualTasksLocationBar>(
        browser_window,
        base::BindRepeating(
            &ContextualTasksPermissionController::PushStateToWebUI,
            weak_factory_.GetWeakPtr()));
  }
#endif
}

ContextualTasksPermissionController::~ContextualTasksPermissionController() =
    default;

void ContextualTasksPermissionController::RegisterWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
#if !BUILDFLAG(IS_ANDROID)
  if (location_bar_) {
    web_contents->RemoveUserData(
        location_bar::LocationBarOverrideData::UserDataKey());
    location_bar::LocationBarOverrideData::CreateForWebContents(
        web_contents, location_bar_.get());
  }
#endif

  // Disconnect stale permission observer and if applicable, register
  // new one based on new web contents.
  prm_observation_.Reset();
  if (auto* prm = permissions::PermissionRequestManager::FromWebContents(
          web_contents)) {
    prm_observation_.Observe(prm);
  }
}

void ContextualTasksPermissionController::UnregisterWebContents(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return;
  }
#if !BUILDFLAG(IS_ANDROID)
  web_contents->RemoveUserData(
      location_bar::LocationBarOverrideData::UserDataKey());
#endif

  if (auto* prm =
          permissions::PermissionRequestManager::FromWebContents(web_contents);
      prm && prm_observation_.IsObservingSource(prm)) {
    prm_observation_.Reset();
  }
}

toolbar_ui_api::mojom::PermissionDashboardStatePtr
ContextualTasksPermissionController::GetState() const {
#if !BUILDFLAG(IS_ANDROID)
  if (location_bar_ && location_bar_->permission_dashboard()) {
    return location_bar_->permission_dashboard()->GetState();
  }
#endif
  // `PermissionDashboardState`'s chip fields are non-nullable, so the "nothing
  // to show" state still needs fully populated (hidden) chips.
  auto state = toolbar_ui_api::mojom::PermissionDashboardState::New();
  state->request_chip = toolbar_ui_api::mojom::PermissionChipState::New();
  state->indicator_chip = toolbar_ui_api::mojom::PermissionChipState::New();
  state->is_divider_visible = false;
  return state;
}

void ContextualTasksPermissionController::PushStateToWebUI() {
  // Coalesce mojo updates. Only send one update at a time.
  if (state_push_pending_) {
    return;
  }

  state_push_pending_ = true;
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&ContextualTasksPermissionController::PushStateToWebUINow,
                     weak_factory_.GetWeakPtr()));
}

void ContextualTasksPermissionController::PushStateToWebUINow() {
  state_push_pending_ = false;

#if !BUILDFLAG(IS_ANDROID)
  if (!location_bar_) {
    return;
  }
  // Resolved on demand rather than cached: the toolbar WebUI is shared by all
  // tasks in the side panel and is torn down and rebuilt independently of this
  // controller, so holding a pointer to it would mean tracking its lifetime.
  if (auto* ui = ContextualTasksUIBase::FromWebContents(
          location_bar_->GetToolbarWebContents())) {
    // `ui` drops the update if this controller's task is not the active one.
    ui->NotifyPermissionDashboardStateChanged(this);
  }
#endif
}

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
  // No state to push to webUI, so do not push state to webUI here.
#endif
}

void ContextualTasksPermissionController::OnChipCollapseAnimationEnded(
    toolbar_ui_api::mojom::LhsChipIdentifier chip_identifier) {
#if !BUILDFLAG(IS_ANDROID)
  if (ContextualTasksPermissionChip* chip = GetChip(chip_identifier)) {
    chip->OnCollapseAnimationEnded();
  }
  // No state to push to webUI, so do not push state to webUI here.
#endif
}

}  // namespace contextual_tasks
