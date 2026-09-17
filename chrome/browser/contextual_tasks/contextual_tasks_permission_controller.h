// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CONTROLLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CONTROLLER_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "build/build_config.h"
#include "components/browser_apis/ui_controllers/toolbar/toolbar_ui_api_data_model.mojom-forward.h"
#include "components/permissions/permission_request_manager.h"
#include "content/public/browser/web_contents_user_data.h"

class BrowserWindowInterface;

namespace content {
class NavigationHandle;
class WebContents;
}  // namespace content

namespace contextual_tasks {

class ContextualTasksLocationBar;
class ContextualTasksPermissionChip;
class ContextualTasksPermissionDashboard;

// Implements the toolbar mojom interface to control
// the dashboard and permission chip for contextual tasks.
class ContextualTasksPermissionController
    : public content::WebContentsUserData<ContextualTasksPermissionController>,
      public permissions::PermissionRequestManager::Observer {
 public:
  ContextualTasksPermissionController(
      const ContextualTasksPermissionController&) = delete;
  ~ContextualTasksPermissionController() override;
  ContextualTasksPermissionController& operator=(
      const ContextualTasksPermissionController&) = delete;

  // permissions::PermissionRequestManager::Observer:
  void OnRequestsFinalized() override;
  void OnPromptRemoved() override;
  void OnNavigation(content::NavigationHandle* navigation_handle) override;
  void OnPermissionRequestManagerDestructed() override;

  virtual toolbar_ui_api::mojom::PermissionDashboardStatePtr GetState() const;
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
  // Use `CreateForWebContents()` (inherited from `WebContentsUserData`) to
  // create instances. `browser_window` may be null, in which case no location
  // bar is created. Protected rather than private so that tests can be a
  // subclass to access.
  ContextualTasksPermissionController(content::WebContents* web_contents,
                                      BrowserWindowInterface* browser_window);

 private:
  friend class content::WebContentsUserData<
      ContextualTasksPermissionController>;
  WEB_CONTENTS_USER_DATA_KEY_DECL();

#if !BUILDFLAG(IS_ANDROID)
  // Returns the chip `chip_identifier` refers to, or null if there is no
  // dashboard (i.e. no location bar).
  ContextualTasksPermissionChip* GetChip(
      toolbar_ui_api::mojom::LhsChipIdentifier chip_identifier);
#endif
#if !BUILDFLAG(IS_ANDROID)
  std::unique_ptr<ContextualTasksLocationBar> location_bar_;
#endif

  base::ScopedObservation<permissions::PermissionRequestManager,
                          permissions::PermissionRequestManager::Observer>
      prm_observation_{this};
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_PERMISSION_CONTROLLER_H_
