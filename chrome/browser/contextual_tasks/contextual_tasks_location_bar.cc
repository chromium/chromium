// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_location_bar.h"

#include <utility>

#include "chrome/browser/contextual_tasks/contextual_tasks_permission_dashboard.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "content/public/browser/web_contents.h"
#include "ui/views/bubble/bubble_border.h"

namespace contextual_tasks {

ContextualTasksLocationBar::ContextualTasksLocationBar(
    BrowserWindowInterface* browser_window,
    base::RepeatingClosure state_changed_callback)
    : browser_window_(browser_window),
      state_changed_callback_(std::move(state_changed_callback)) {
  // No WebUI delegate is passed: the models are only consulted for the
  // indicator chip, whose state reaches the WebUI through the permission
  // dashboard instead.

  // Generates the default set of content setting models.
  content_setting_image_control_.Init();

  permission_dashboard_ =
      std::make_unique<ContextualTasksPermissionDashboard>(this);
  permission_dashboard_controller_ =
      std::make_unique<PermissionDashboardController>(
          /*location_bar=*/this,
          /*content_settings_image_delegate=*/this,
          /*permission_dashboard=*/permission_dashboard_.get());
}

ContextualTasksLocationBar::~ContextualTasksLocationBar() {
  // Disconnect the callback before `permission_dashboard_controller_` is
  // destroyed: `~ChipController()` hides its chips during teardown, which
  // calls `OnChanged()`.
  state_changed_callback_.Reset();
}

ContextualTasksSidePanelCoordinator*
ContextualTasksLocationBar::GetCoordinator() const {
  return browser_window_ ? ContextualTasksSidePanelCoordinator::Get(
                               browser_window_->GetUnownedUserDataHost())
                         : nullptr;
}

content::WebContents* ContextualTasksLocationBar::GetWebContents() {
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  return coordinator ? coordinator->GetActiveWebContents() : nullptr;
}

content::WebContents* ContextualTasksLocationBar::GetToolbarWebContents()
    const {
  ContextualTasksSidePanelCoordinator* coordinator = GetCoordinator();
  return coordinator ? coordinator->GetToolbarWebContents() : nullptr;
}

BrowserWindowInterface* ContextualTasksLocationBar::GetBrowser() {
  return browser_window_;
}

bool ContextualTasksLocationBar::IsEditingOrEmpty() const {
  return false;
}

ui::TrackedElement* ContextualTasksLocationBar::GetAnchorOrNull() {
  return nullptr;
}

void ContextualTasksLocationBar::InvalidateLayout() {}

bool ContextualTasksLocationBar::IsDrawn() const {
  return GetToolbarWebContents() != nullptr;
}

bool ContextualTasksLocationBar::IsFullscreen() const {
  return false;
}

std::optional<bubble_anchor_util::AnchorConfiguration>
ContextualTasksLocationBar::GetChipAnchor() {
  if (permission_dashboard_) {
    return {{permission_dashboard_->GetAnchor(), std::nullopt,
             views::BubbleBorder::TOP_LEFT}};
  }
  return std::nullopt;
}

ChipController* ContextualTasksLocationBar::GetChipController() {
  return permission_dashboard_controller_
             ? permission_dashboard_controller_->request_chip_controller()
             : nullptr;
}

PermissionDashboardController*
ContextualTasksLocationBar::GetPermissionDashboardController() {
  return permission_dashboard_controller_.get();
}

void ContextualTasksLocationBar::OnChanged() {
  // Single funnel for "something the toolbar renders has changed". The
  // permission chips and dashboard both send events here; the callback forwards
  // to `ContextualTasksPermissionController`, which coalesces and pushes to the
  // WebUI.
  if (state_changed_callback_) {
    state_changed_callback_.Run();
  }
}

void ContextualTasksLocationBar::UpdateContentSettingsIcons() {
  if (!GetWebContents()) {
    return;
  }
  if (content_setting_image_control_.UpdatePermissionDashboard(
          permission_dashboard_controller_.get())) {
    OnChanged();
  }
}

bool ContextualTasksLocationBar::ShouldHideContentSettingImage() {
  return false;
}

content::WebContents*
ContextualTasksLocationBar::GetContentSettingWebContents() {
  return nullptr;
}

ContentSettingBubbleModelDelegate*
ContextualTasksLocationBar::GetContentSettingBubbleModelDelegate() {
  return nullptr;
}

}  // namespace contextual_tasks
