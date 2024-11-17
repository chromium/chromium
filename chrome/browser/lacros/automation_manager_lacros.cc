// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/automation_manager_lacros.h"

#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chromeos/lacros/lacros_service.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/browser/api/automation_internal/automation_internal_api.h"
#include "ui/accessibility/ax_tree_id.h"

AutomationManagerLacros::AutomationManagerLacros() {
  chromeos::LacrosService* impl = chromeos::LacrosService::Get();
  if (!impl->IsAvailable<crosapi::mojom::AutomationFactory>())
    return;

  impl->GetRemote<crosapi::mojom::AutomationFactory>()->BindAutomation(
      automation_client_receiver_.BindNewPipeAndPassRemoteWithVersion(),
      automation_remote_.BindNewPipeAndPassReceiver());

  extensions::AutomationEventRouter::GetInstance()->RegisterRemoteRouter(this);
}

AutomationManagerLacros::~AutomationManagerLacros() {
  chromeos::LacrosService* impl = chromeos::LacrosService::Get();
  if (!impl->IsAvailable<crosapi::mojom::AutomationFactory>())
    return;

  extensions::AutomationEventRouter::GetInstance()->RegisterRemoteRouter(
      nullptr);
}

void AutomationManagerLacros::DispatchAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const gfx::Point& mouse_location,
    const std::vector<ui::AXEvent>& events) {
  if (!tree_id.token())
    return;

  // TODO: we probably don't want to check every time but only once and cache
  // the value(s). Also, we need to check all accessibility enums, structs
  // reachable from AXTreeUpdate and AXEvent.
  int remote_version = chromeos::LacrosService::Get()
                           ->GetInterfaceVersion<crosapi::mojom::Automation>();
  if (remote_version < 0 ||
      crosapi::mojom::Automation::kDispatchAccessibilityEventsMinVersion >
          static_cast<uint32_t>(remote_version)) {
    return;
  }

  DCHECK(automation_remote_);
  automation_remote_->DispatchAccessibilityEvents(*tree_id.token(), updates,
                                                  mouse_location, events);
}

void AutomationManagerLacros::DispatchAccessibilityLocationChange(
    const ui::AXTreeID& tree_id,
    const ui::AXLocationChange& details) {
  if (!tree_id.token())
    return;

  DCHECK(automation_remote_);
  automation_remote_->DispatchAccessibilityLocationChange(
      *tree_id.token(), details.id, details.new_location);
}

void AutomationManagerLacros::DispatchTreeDestroyedEvent(ui::AXTreeID tree_id) {
  if (!tree_id.token())
    return;

  DCHECK(automation_remote_);
  automation_remote_->DispatchTreeDestroyedEvent(*(tree_id.token()));
}

void AutomationManagerLacros::DispatchActionResult(
    const ui::AXActionData& data,
    bool result,
    content::BrowserContext* browser_context) {
  DCHECK(automation_remote_);
  automation_remote_->DispatchActionResult(data, result);
}

void AutomationManagerLacros::DispatchGetTextLocationDataResult(
    const ui::AXActionData& data,
    const std::optional<gfx::Rect>& rect) {
  // Unsupported by Laros.
}

void AutomationManagerLacros::Enable() {
  AutomationManagerAura::GetInstance()->Enable();
}

void AutomationManagerLacros::EnableTree(const base::UnguessableToken& token) {
  ui::AXTreeID tree_id = ui::AXTreeID::FromToken(token);
  extensions::AutomationInternalEnableTreeFunction::EnableTree(
      tree_id, /*extension_id=*/"");
}

void AutomationManagerLacros::Disable() {
  AutomationManagerAura::GetInstance()->Disable();
}

void AutomationManagerLacros::PerformActionDeprecated(
    const base::UnguessableToken& token,
    int32_t automation_node_id,
    const std::string& action_type,
    int32_t request_id,
    base::Value::Dict optional_args) {}

void AutomationManagerLacros::PerformAction(
    const ui::AXActionData& action_data) {
  extensions::AutomationInternalPerformActionFunction::PerformAction(
      action_data, /*extension=*/nullptr, /*automation_info=*/nullptr);
}

void AutomationManagerLacros::NotifyAllAutomationExtensionsGone() {
  extensions::AutomationEventRouter::GetInstance()
      ->NotifyAllAutomationExtensionsGone();
}

void AutomationManagerLacros::NotifyExtensionListenerAdded() {
  extensions::AutomationEventRouter::GetInstance()
      ->NotifyExtensionListenerAdded();
}
