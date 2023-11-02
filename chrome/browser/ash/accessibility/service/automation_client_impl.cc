// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/automation_client_impl.h"
#include "chrome/browser/accessibility/service/accessibility_service_router.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/browser/api/automation_internal/automation_internal_api.h"

namespace ash {

AutomationClientImpl::AutomationClientImpl() = default;

AutomationClientImpl::~AutomationClientImpl() {
  if (!bound_)
    return;

  extensions::AutomationEventRouter::GetInstance()->RegisterRemoteRouter(
      nullptr);
}

void AutomationClientImpl::Bind(ax::AccessibilityServiceRouter* router) {
  // Launches the service if it wasn't running yet.
  // Development note (crbug.com/1355633): Using the remote router means
  // extensions don't get a11y events when AutomationClientImpl is bound, so
  // accessibility features built as component extensions are broken when the
  // service is running.
  DCHECK(!bound_);
  bound_ = true;
  extensions::AutomationEventRouter::GetInstance()->RegisterRemoteRouter(this);
  router->BindAutomationWithClient(
      automation_client_receiver_.BindNewPipeAndPassRemote(),
      automation_.BindNewPipeAndPassReceiver());
}

void AutomationClientImpl::DispatchAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    std::vector<ui::AXTreeUpdate> updates,
    const gfx::Point& mouse_location,
    std::vector<ui::AXEvent> events) {
  DCHECK(tree_id != ui::AXTreeIDUnknown());
  if (tree_id == ui::AXTreeIDUnknown() || !automation_.is_bound())
    return;
  // TODO(crbug.com/1355633): Send to AccessibilityService.
  // automation_->DispatchAccessibilityEvents(*tree_id.token(), updates,
  //                                         mouse_location, events);
}

void AutomationClientImpl::DispatchAccessibilityLocationChange(
    const ExtensionMsg_AccessibilityLocationChangeParams& params) {
  ui::AXTreeID tree_id = params.tree_id;
  if (!tree_id.token() || !automation_.is_bound())
    return;
  // TODO(crbug.com/1355633): Send to AccessibilityService.
  // automation_->DispatchAccessibilityLocationChange(*tree_id.token(),
  // params.id,
  //                                                  params.new_location);
}
void AutomationClientImpl::DispatchTreeDestroyedEvent(
    ui::AXTreeID tree_id,
    content::BrowserContext* browser_context) {
  if (!tree_id.token() || !automation_.is_bound())
    return;
  // TODO(crbug.com/1355633): Send to AccessibilityService.
  // automation_->DispatchTreeDestroyedEvent(*(tree_id.token()));
}

void AutomationClientImpl::DispatchActionResult(
    const ui::AXActionData& data,
    bool result,
    content::BrowserContext* browser_context) {
  if (!automation_.is_bound())
    return;
  // TODO(crbug.com/1355633): Send to AccessibilityService.
  // automation_->DispatchActionResult(data, result);
}

void AutomationClientImpl::DispatchGetTextLocationDataResult(
    const ui::AXActionData& data,
    const absl::optional<gfx::Rect>& rect) {
  // TODO(crbug.com/1355633): Send to AccessibilityService.
  // automation_->DispatchGetTextLocationDataResult(data, rect);
}

void AutomationClientImpl::Enable() {
  // Enable automation for all of Desktop.
  AutomationManagerAura::GetInstance()->Enable();
}

void AutomationClientImpl::Disable() {
  // Disable automation.
  AutomationManagerAura::GetInstance()->Disable();
}

void AutomationClientImpl::EnableTree(const base::UnguessableToken& tree_id) {
  ui::AXTreeID ax_tree_id = ui::AXTreeID::FromToken(tree_id);
  // TODO(crbug.com/1355633): Refactor logic from extensions namespace to a
  // common location.
  extensions::AutomationInternalEnableTreeFunction::EnableTree(
      ax_tree_id, /*extension_id=*/"");
}

void AutomationClientImpl::PerformAction(const ui::AXActionData& data) {
  // TODO(crbug.com/1355633): Refactor logic from extensions namespace to a
  // common location.
  extensions::AutomationInternalPerformActionFunction::PerformAction(
      data, /*extension=*/nullptr, /*automation_info=*/nullptr);
}

}  // namespace ash
