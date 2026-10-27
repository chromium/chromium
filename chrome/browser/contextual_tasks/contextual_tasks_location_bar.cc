// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_location_bar.h"

#include <utility>

#include "chrome/browser/contextual_tasks/contextual_tasks_side_panel_coordinator.h"
#include "chrome/browser/ui/browser_element_identifiers.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/views/bubble_anchor_util_views.h"
#include "chrome/browser/ui/views/permissions/chip/chip_controller.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_interface.h"
#include "chrome/browser/ui/views/permissions/chip/permission_chip_view.h"
#include "chrome/browser/ui/views/permissions/chip/permission_dashboard_controller.h"
#include "chrome/browser/ui/views/permissions/chip/webui_permission_dashboard.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/interaction/element_tracker.h"
#include "ui/views/bubble/bubble_border.h"
#include "ui/webui/tracked_element/tracked_element_handler.h"
#include "ui/webui/tracked_element/tracked_element_handler_document_singleton.h"

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

  permission_dashboard_ = std::make_unique<WebUIPermissionDashboard>(this);
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
  // Controller of dashboard then dashboard are destroyed automatically in that
  // order.
}

ContextualTasksPanelController* ContextualTasksLocationBar::GetCoordinator()
    const {
  return ContextualTasksPanelController::From(browser_window_);
}

content::WebContents* ContextualTasksLocationBar::GetWebContents() {
  return web_contents_.get();
}

content::WebContents* ContextualTasksLocationBar::GetToolbarWebContents()
    const {
  ContextualTasksPanelController* coordinator = GetCoordinator();
  return coordinator ? coordinator->GetToolbarWebContents() : nullptr;
}

BrowserWindowInterface* ContextualTasksLocationBar::GetBrowser() {
  return browser_window_;
}

bool ContextualTasksLocationBar::IsEditingOrEmpty() const {
  return false;
}

ui::TrackedElement* ContextualTasksLocationBar::GetAnchorOrNull() {
  if (ui::ElementContext context = GetElementContext()) {
    return ui::ElementTracker::GetElementTracker()->GetFirstMatchingElement(
        kContextualTasksWebUIToolbarElementId, context);
  }
  return nullptr;
}

ui::ElementContext ContextualTasksLocationBar::GetElementContext() const {
  content::WebContents* webui_contents = GetToolbarWebContents();
  if (webui_contents && webui_contents->GetPrimaryMainFrame()) {
    if (auto handler = ui::TrackedElementHandlerDocumentSingleton::GetOrCreate(
            webui_contents->GetPrimaryMainFrame())) {
      return handler->context();
    }
  }
  return ui::ElementContext();
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
  if (!permission_dashboard_) {
    return std::nullopt;
  }
  PermissionChipInterface* request_chip =
      permission_dashboard_->GetRequestChip();
  if (request_chip && request_chip->GetVisible()) {
    bubble_anchor_util::AnchorConfiguration configuration{
        request_chip->GetAnchor(), std::nullopt, views::BubbleBorder::TOP_LEFT};
    // Highlight the request chip by ID, looked up in the toolbar WebUI's
    // element context. Without the context, the ID would be resolved in the
    // browser window's context and highlight the main toolbar's chip whenever
    // that one is showing. Unlike relying on the anchor, highlighting by ID
    // also re-applies the highlight if the WebUI re-registers the chip while
    // the bubble is open.
    if (ui::ElementContext context = GetElementContext()) {
      configuration.highlighted_element =
          PermissionChipView::kPermissionRequestChipElementId;
      configuration.highlighted_element_context = context;
    }
    return configuration;
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

void ContextualTasksLocationBar::Update(content::WebContents* contents) {
  // Reset permission chip if it is there and then update chips/dashboard.
  if (web_contents_.get() != contents) {
    if (auto* chip_controller = GetChipController()) {
      chip_controller->ResetPermissionPromptChip();
    }
    // Mirrors `WebUILocationBar::Update()`: invalidate the chips' state tokens
    // so that click IPCs still in flight from the previous task are dropped,
    // even if a chip stays visible across the switch. `OnChanged()` below
    // pushes the new tokens to the WebUI.
    if (permission_dashboard_) {
      permission_dashboard_->ResetTabState();
    }
    web_contents_ = contents ? contents->GetWeakPtr() : nullptr;
  }
  // Re-evaluate the activity indicator models (camera/mic/sensors) for the
  // new `web_contents_` and push the result into the permission dashboard.
  // `UpdatePermissionDashboard` gets web contents from location bar
  // dynamically, so this must be called after web contents are set in location
  // bar.
  content_setting_image_control_.UpdatePermissionDashboard(
      permission_dashboard_controller_.get());
  OnChanged();
}

bool ContextualTasksLocationBar::ShouldHideContentSettingImage() {
  return false;
}

content::WebContents*
ContextualTasksLocationBar::GetContentSettingWebContents() {
  return GetWebContents();
}

ContentSettingBubbleModelDelegate*
ContextualTasksLocationBar::GetContentSettingBubbleModelDelegate() {
  return nullptr;
}

}  // namespace contextual_tasks
