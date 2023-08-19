// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/service/automation_client_impl.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/browser/api/automation_internal/automation_internal_api.h"
#include "mojo/public/cpp/bindings/pending_associated_remote.h"

namespace ash {

AutomationClientImpl::AutomationClientImpl() = default;

AutomationClientImpl::~AutomationClientImpl() {
  if (!bound_)
    return;

  extensions::AutomationEventRouter::GetInstance()->RegisterRemoteRouter(
      nullptr);
}

void AutomationClientImpl::Bind(
    mojo::PendingAssociatedRemote<ax::mojom::Automation> automation,
    mojo::PendingReceiver<ax::mojom::AutomationClient> automation_client) {
  // Launches the service if it wasn't running yet.
  // Development note (crbug.com/1355633): Using the remote router means
  // extensions don't get a11y events when AutomationClientImpl is bound, so
  // accessibility features built as component extensions are broken when the
  // service is running.
  if (!bound_) {
    bound_ = true;
    extensions::AutomationEventRouter::GetInstance()->RegisterRemoteRouter(
        this);
  }
  automation_remotes_.Add(std::move(automation));
  automation_client_receivers_.Add(this, std::move(automation_client));
}

void AutomationClientImpl::DispatchAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    std::vector<ui::AXTreeUpdate> updates,
    const gfx::Point& mouse_location,
    std::vector<ui::AXEvent> events) {
  DCHECK(tree_id != ui::AXTreeIDUnknown());
  if (tree_id == ui::AXTreeIDUnknown())
    return;
  for (auto& remote : automation_remotes_) {
    remote->DispatchAccessibilityEvents(tree_id, updates, mouse_location,
                                        events);
  }
}

void AutomationClientImpl::DispatchAccessibilityLocationChange(
    const content::AXLocationChangeNotificationDetails& details) {
  ui::AXTreeID tree_id = details.ax_tree_id;
  if (tree_id == ui::AXTreeIDUnknown())
    return;
  for (auto& remote : automation_remotes_) {
    remote->DispatchAccessibilityLocationChange(tree_id, details.id,
                                                details.new_location);
  }
}
void AutomationClientImpl::DispatchTreeDestroyedEvent(ui::AXTreeID tree_id) {
  if (tree_id == ui::AXTreeIDUnknown())
    return;
  // TODO(crbug.com/1355633): Send to AccessibilityService.
  // for (auto& remote : automation_remotes_) {
  //   remote->DispatchTreeDestroyedEvent(tree_id);
  // }
}

void AutomationClientImpl::DispatchActionResult(
    const ui::AXActionData& data,
    bool result,
    content::BrowserContext* browser_context) {
  // TODO(crbug.com/1355633): Send to AccessibilityService.
  // for (auto& remote : automation_remotes_) {
  //   remote->DispatchActionResult(data, result);
  // }
}

void AutomationClientImpl::DispatchGetTextLocationDataResult(
    const ui::AXActionData& data,
    const absl::optional<gfx::Rect>& rect) {
  // TODO(crbug.com/1355633): Send to AccessibilityService.
  // for (auto& remote : automation_remotes_) {
  //   remote->DispatchGetTextLocationDataResult(data, rect);
  // }
}

void AutomationClientImpl::Enable(EnableCallback callback) {
  // Enable automation for all of Desktop.
  AutomationManagerAura::GetInstance()->Enable();
  std::move(callback).Run(AutomationManagerAura::GetInstance()->ax_tree_id());
}

void AutomationClientImpl::Disable() {
  // Disable automation.
  AutomationManagerAura::GetInstance()->Disable();
}

void AutomationClientImpl::EnableTree(const ui::AXTreeID& tree_id) {
  // TODO(crbug.com/1355633): Refactor logic from extensions namespace to a
  // common location.
  extensions::AutomationInternalEnableTreeFunction::EnableTree(
      tree_id, /*extension_id=*/"");
}

void AutomationClientImpl::PerformAction(const ui::AXActionData& data) {
  // TODO(crbug.com/1355633): Refactor logic from extensions namespace to a
  // common location.
  extensions::AutomationInternalPerformActionFunction::PerformAction(
      data, /*extension=*/nullptr, /*automation_info=*/nullptr);
}

}  // namespace ash
