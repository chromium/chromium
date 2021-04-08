// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/automation_manager_lacros.h"

#include "base/pickle.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/browser/api/automation_internal/automation_internal_api.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_tree_id.h"

AutomationManagerLacros::AutomationManagerLacros() {
  chromeos::LacrosChromeServiceImpl* impl =
      chromeos::LacrosChromeServiceImpl::Get();
  if (!impl->IsAvailable<crosapi::mojom::AutomationFactory>())
    return;

  impl->GetRemote<crosapi::mojom::AutomationFactory>()->BindAutomation(
      automation_client_receiver_.BindNewPipeAndPassRemote(),
      automation_remote_.BindNewPipeAndPassReceiver());

  extensions::AutomationEventRouter::GetInstance()->RegisterRemoteRouter(this);
}

AutomationManagerLacros::~AutomationManagerLacros() {
  chromeos::LacrosChromeServiceImpl* impl =
      chromeos::LacrosChromeServiceImpl::Get();
  if (!impl->IsAvailable<crosapi::mojom::AutomationFactory>())
    return;

  extensions::AutomationEventRouter::GetInstance()->RegisterRemoteRouter(
      nullptr);
}

void AutomationManagerLacros::DispatchAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    std::vector<ui::AXTreeUpdate> updates,
    const gfx::Point& mouse_location,
    std::vector<ui::AXEvent> events) {
  if (!automation_remote_)
    return;

  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = tree_id;
  event_bundle.updates = std::move(updates);
  event_bundle.mouse_location = mouse_location;
  event_bundle.events = std::move(events);
  base::Pickle pickle;
  IPC::ParamTraits<ExtensionMsg_AccessibilityEventBundleParams>::Write(
      &pickle, event_bundle);
  std::string result(static_cast<const char*>(pickle.data()), pickle.size());
  automation_remote_->ReceiveEventPrototype(std::move(result), false,
                                            base::UnguessableToken::Create(),
                                            std::string());
}

void AutomationManagerLacros::DispatchAccessibilityLocationChange(
    const ExtensionMsg_AccessibilityLocationChangeParams& params) {
  // TODO(https://crbug.com/1185764): Implement me.
}

void AutomationManagerLacros::DispatchTreeDestroyedEvent(
    ui::AXTreeID tree_id,
    content::BrowserContext* browser_context) {
  if (!tree_id.token())
    return;

  if (!automation_remote_)
    return;

  automation_remote_->DispatchTreeDestroyedEvent(*(tree_id.token()));
}

void AutomationManagerLacros::DispatchActionResult(
    const ui::AXActionData& data,
    bool result,
    content::BrowserContext* browser_context) {
  // TODO(https://crbug.com/1185764): Implement me.
}
void AutomationManagerLacros::DispatchGetTextLocationDataResult(
    const ui::AXActionData& data,
    const base::Optional<gfx::Rect>& rect) {
  // TODO(https://crbug.com/1185764): Implement me.
}

void AutomationManagerLacros::Enable() {
  AutomationManagerAura::GetInstance()->Enable();
}

void AutomationManagerLacros::EnableTree(const base::UnguessableToken& token) {
  ui::AXTreeID tree_id = ui::AXTreeID::FromToken(token);
  extensions::AutomationInternalEnableTreeFunction::EnableTree(
      tree_id, /*extension_id=*/"");
}

void AutomationManagerLacros::PerformActionPrototype(
    const base::UnguessableToken& token,
    int32_t automation_node_id,
    const std::string& action_type,
    int32_t request_id,
    base::Value optional_args) {
  ui::AXTreeID tree_id = ui::AXTreeID::FromToken(token);
  const base::DictionaryValue& dict =
      base::Value::AsDictionaryValue(optional_args);
  extensions::AutomationInternalPerformActionFunction::PerformAction(
      tree_id, automation_node_id, action_type, request_id, dict,
      /*extension_id=*/"", /*extension=*/nullptr, /*automation_info=*/nullptr);
}
