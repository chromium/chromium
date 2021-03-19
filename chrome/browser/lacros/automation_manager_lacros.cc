// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/automation_manager_lacros.h"

#include "base/pickle.h"
#include "chrome/browser/ui/aura/accessibility/automation_manager_aura.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chromeos/lacros/lacros_chrome_service_impl.h"
#include "extensions/browser/api/automation_internal/automation_event_router.h"
#include "extensions/browser/api/automation_internal/automation_internal_api.h"
#include "extensions/common/extension_messages.h"
#include "ui/accessibility/ax_tree_id.h"
#include "ui/aura/window.h"
#include "ui/aura/window_tree_host.h"
#include "ui/aura/window_tree_host_platform.h"
#include "ui/platform_window/platform_window.h"

AutomationManagerLacros::AutomationManagerLacros() {
  chromeos::LacrosChromeServiceImpl* impl =
      chromeos::LacrosChromeServiceImpl::Get();
  if (!impl->IsAutomationAvailable())
    return;
  id_ = base::UnguessableToken::Create();
  impl->automation_remote()->RegisterAutomationClient(
      receiver_.BindNewPipeAndPassRemote(), id_);

  extensions::AutomationEventRouter::GetInstance()->RegisterRemoteRouter(this);
}

AutomationManagerLacros::~AutomationManagerLacros() {
  chromeos::LacrosChromeServiceImpl* impl =
      chromeos::LacrosChromeServiceImpl::Get();
  if (!impl->IsAutomationAvailable())
    return;

  extensions::AutomationEventRouter::GetInstance()->RegisterRemoteRouter(
      nullptr);
}

void AutomationManagerLacros::DispatchAccessibilityEvents(
    const ui::AXTreeID& tree_id,
    std::vector<ui::AXTreeUpdate> updates,
    const gfx::Point& mouse_location,
    std::vector<ui::AXEvent> events) {
  ExtensionMsg_AccessibilityEventBundleParams event_bundle;
  event_bundle.tree_id = tree_id;
  event_bundle.updates = std::move(updates);
  event_bundle.mouse_location = mouse_location;
  event_bundle.events = std::move(events);

  // TODO(https://crbug.com/1185764): We'll likely want to push this up to
  // AutomationManagerAura/AXTreeViews where we can directly retrieve the root
  // window given a view or widget on which we're serializing accessibility
  // data.
  BrowserList* list = BrowserList::GetInstance();
  std::string window_id;
  if (!list->empty()) {
    Browser* browser = list->get(0);
    aura::Window* window = browser->window()->GetNativeWindow();

    // On desktop aura there is one WindowTreeHost per top-level window.
    aura::WindowTreeHost* window_tree_host = window->GetHost();
    DCHECK(window_tree_host);
    // Lacros is based on Ozone/Wayland, which uses PlatformWindow and
    // aura::WindowTreeHostPlatform.
    aura::WindowTreeHostPlatform* window_tree_host_platform =
        static_cast<aura::WindowTreeHostPlatform*>(window_tree_host);
    window_id =
        window_tree_host_platform->platform_window()->GetWindowUniqueId();
  }
  bool is_root =
      tree_id ==
      AutomationManagerAura::GetInstance()->get_root_tree_id_deprecated();
  base::Pickle pickle;
  IPC::ParamTraits<ExtensionMsg_AccessibilityEventBundleParams>::Write(
      &pickle, event_bundle);
  std::string result(static_cast<const char*>(pickle.data()), pickle.size());
  chromeos::LacrosChromeServiceImpl::Get()
      ->automation_remote()
      ->ReceiveEventPrototype(std::move(result), is_root, id_, window_id);
}

void AutomationManagerLacros::DispatchAccessibilityLocationChange(
    const ExtensionMsg_AccessibilityLocationChangeParams& params) {
  // TODO(https://crbug.com/1185764): Implement me.
}
void AutomationManagerLacros::DispatchTreeDestroyedEvent(
    ui::AXTreeID tree_id,
    content::BrowserContext* browser_context) {
  // TODO(https://crbug.com/1185764): Implement me.
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
