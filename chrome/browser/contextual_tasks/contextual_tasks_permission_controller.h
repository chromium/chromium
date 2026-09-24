// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom-forward.h"
#include "components/permissions/permission_request_manager.h"

class BrowserWindowInterface;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace contextual_tasks {

class ContextualTasksLocationBar;
class ContextualTasksPermissionChip;

// Implements the toolbar mojom interface to control
// the dashboard and permission chip for the contextual tasks side panel.
// Owned by `ContextualTasksSidePanelCoordinator` (one instance per side panel).
class ContextualTasksPermissionController
    : public permissions::PermissionRequestManager::Observer {
 public:
  // `browser_window` may be null in unit tests, in which case no location bar
  // is created.
  explicit ContextualTasksPermissionController(
      BrowserWindowInterface* browser_window);
  ContextualTasksPermissionController(
      const ContextualTasksPermissionController&) = delete;
  ~ContextualTasksPermissionController() override;
  ContextualTasksPermissionController& operator=(
      const ContextualTasksPermissionController&) = delete;

  // Attaches `LocationBarOverrideData` to `web_contents` pointing to this
  // controller's single `ContextualTasksLocationBar`.
  void RegisterWebContents(content::WebContents* web_contents);

  // Removes `LocationBarOverrideData` from `web_contents` when it is detached
  // from the side panel (e.g. moved to a full browser tab).
  void UnregisterWebContents(content::WebContents* web_contents);

  // permissions::PermissionRequestManager::Observer:
  void OnRequestsFinalized() override;
  void OnPromptRemoved() override;
  void OnNavigation(content::NavigationHandle* navigation_handle) override;
  void OnPermissionRequestManagerDestructed() override;

  virtual toolbar_ui_api::mojom::PermissionDashboardStatePtr GetState() const;

  // Schedules a push of the current dashboard state to the toolbar WebUI.
  //
  // Coalesced: a single logical update (e.g. `PermissionDashboardController`
  // setting an icon, message, theme and visibility in sequence) mutates the
  // chips many times, and each mutation funnels through here. Batching them
  // into one task collapses the burst into a single IPC carrying only the
  // final state.
  void PushStateToWebUI();

  // Chip Interactions (called from Mojo via ContextualTasksUI):
  virtual void OnChipClicked(
      toolbar_ui_api::mojom::LhsChipIdentifier chip_identifier,
      bool is_mouse_interaction);
  virtual void OnChipExpandAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier chip_identifier);
  virtual void OnChipCollapseAnimationEnded(
      toolbar_ui_api::mojom::LhsChipIdentifier chip_identifier);

#if !BUILDFLAG(IS_ANDROID)
  ContextualTasksLocationBar* GetLocationBarForTesting() {
    return location_bar_.get();
  }
#endif

 protected:
  // Resolves the toolbar WebUI and hands it the current state. Does nothing if
  // the side panel is gone.
  virtual void PushStateToWebUINow();

 private:
#if !BUILDFLAG(IS_ANDROID)
  // Returns the chip `chip_identifier` refers to, or null if there is no
  // dashboard (i.e. no location bar).
  ContextualTasksPermissionChip* GetChip(
      toolbar_ui_api::mojom::LhsChipIdentifier chip_identifier);

  std::unique_ptr<ContextualTasksLocationBar> location_bar_;
#endif

  // Whether a `PushStateToWebUINow()` task is already queued.
  bool state_push_pending_ = false;

  base::ScopedObservation<permissions::PermissionRequestManager,
                          permissions::PermissionRequestManager::Observer>
      prm_observation_{this};

  base::WeakPtrFactory<ContextualTasksPermissionController> weak_factory_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CONTROLLER_H_
