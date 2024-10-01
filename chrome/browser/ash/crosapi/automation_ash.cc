// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/automation_ash.h"

#include "ui/accessibility/ax_location_and_scroll_updates.h"
#include "ui/accessibility/ax_tree_id.h"

namespace crosapi {

AutomationAsh::AutomationAsh() {
  ui::AXActionHandlerRegistry::GetInstance()->AddObserver(this);
}

AutomationAsh::~AutomationAsh() {
  ui::AXActionHandlerRegistry::GetInstance()->RemoveObserver(this);
}

void AutomationAsh::BindReceiverDeprecated(
    mojo::PendingReceiver<mojom::Automation> pending_receiver) {}

void AutomationAsh::BindReceiver(
    mojo::PendingReceiver<mojom::AutomationFactory> pending_receiver) {
  automation_factory_receivers_.Add(this, std::move(pending_receiver));

  if (!automation_event_router_observer_.IsObserving()) {
    automation_event_router_observer_.Observe(
        extensions::AutomationEventRouter::GetInstance());
  }
}

void AutomationAsh::EnableDesktop() {
  desktop_enabled_ = true;
  for (auto& client : automation_client_remotes_) {
    client->Enable();
  }
}

void AutomationAsh::EnableTree(const ui::AXTreeID& tree_id) {
  if (!tree_id.token().has_value())
    return;

  for (auto& client : automation_client_remotes_) {
    client->EnableTree(tree_id.token().value());
  }
}

void AutomationAsh::Disable() {
  for (auto& client : automation_client_remotes_) {
    client->Disable();
  }
  desktop_enabled_ = false;
}

void AutomationAsh::DispatchAccessibilityEvents(
    const base::UnguessableToken& tree_id,
    const std::vector<ui::AXTreeUpdate>& updates,
    const gfx::Point& mouse_location,
    const std::vector<ui::AXEvent>& events) {
  extensions::AutomationEventRouter::GetInstance()->DispatchAccessibilityEvents(
      ui::AXTreeID::FromToken(tree_id), updates, mouse_location, events);
}

void AutomationAsh::DispatchAccessibilityLocationChange(
    const base::UnguessableToken& tree_id,
    int32_t node_id,
    const ui::AXRelativeBounds& bounds) {
  ui::AXLocationChange details;
  details.id = node_id;
  details.new_location = bounds;
  ui::AXTreeID ui_tree_id = ui::AXTreeID::FromToken(tree_id);
  extensions::AutomationEventRouter::GetInstance()
      ->DispatchAccessibilityLocationChange(ui_tree_id, details);
}

void AutomationAsh::DispatchTreeDestroyedEvent(
    const base::UnguessableToken& tree_id) {
  extensions::AutomationEventRouter::GetInstance()->DispatchTreeDestroyedEvent(
      ui::AXTreeID::FromToken(tree_id));
}

void AutomationAsh::DispatchActionResult(
    const ui::AXActionData& already_handled_action_data,
    bool result) {
  extensions::AutomationEventRouter::GetInstance()->DispatchActionResult(
      already_handled_action_data, result);
}

// Forwards an action to all crosapi clients. This has no effect on production
// builds of chrome. It exists for prototyping for developers.
void AutomationAsh::PerformAction(const ui::AXActionData& action_data) {
  for (auto& client : automation_client_remotes_)
    client->PerformAction(action_data);
}

void AutomationAsh::BindAutomation(
    mojo::PendingRemote<crosapi::mojom::AutomationClient> automation_client,
    mojo::PendingReceiver<crosapi::mojom::Automation> automation) {
  mojo::Remote<mojom::AutomationClient> remote(std::move(automation_client));

  if (desktop_enabled_) {
    remote->Enable();
  } else {
    remote->Disable();
  }

  automation_client_remotes_.Add(std::move(remote));
  automation_receivers_.Add(this, std::move(automation));
}

void AutomationAsh::AllAutomationExtensionsGone() {
  for (auto& client : automation_client_remotes_)
    client->NotifyAllAutomationExtensionsGone();
}

void AutomationAsh::ExtensionListenerAdded() {
  for (auto& client : automation_client_remotes_)
    client->NotifyExtensionListenerAdded();
}

}  // namespace crosapi
