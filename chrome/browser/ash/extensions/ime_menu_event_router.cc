// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/ime_menu_event_router.h"

#include <memory>
#include <vector>

#include "base/values.h"
#include "chrome/browser/ash/extensions/input_method_api.h"
#include "chrome/common/extensions/api/input_method_private.h"
#include "content/public/browser/browser_context.h"
#include "extensions/browser/event_router.h"

namespace chromeos {

namespace input_method = ::ash::input_method;
namespace input_method_private = extensions::api::input_method_private;
namespace OnImeMenuActivationChanged =
    extensions::api::input_method_private::OnImeMenuActivationChanged;
namespace OnImeMenuListChanged =
    extensions::api::input_method_private::OnImeMenuListChanged;
namespace OnImeMenuItemsChanged =
    extensions::api::input_method_private::OnImeMenuItemsChanged;

ExtensionImeMenuEventRouter::ExtensionImeMenuEventRouter(
    content::BrowserContext* context)
    : context_(context) {
  input_method::InputMethodManager::Get()->AddImeMenuObserver(this);
}

ExtensionImeMenuEventRouter::~ExtensionImeMenuEventRouter() {
  input_method::InputMethodManager::Get()->RemoveImeMenuObserver(this);
}

void ExtensionImeMenuEventRouter::ImeMenuActivationChanged(bool activation) {
  extensions::EventRouter* router = extensions::EventRouter::Get(context_);

  if (!router->HasEventListener(OnImeMenuActivationChanged::kEventName))
    return;

  base::Value::List args;
  args.Append(activation);

  // The router will only send the event to extensions that are listening.
  auto event = std::make_unique<extensions::Event>(
      extensions::events::INPUT_METHOD_PRIVATE_ON_IME_MENU_ACTIVATION_CHANGED,
      OnImeMenuActivationChanged::kEventName, std::move(args), context_);
  router->BroadcastEvent(std::move(event));
}

void ExtensionImeMenuEventRouter::ImeMenuListChanged() {
  extensions::EventRouter* router = extensions::EventRouter::Get(context_);

  if (!router->HasEventListener(OnImeMenuListChanged::kEventName))
    return;

  // The router will only send the event to extensions that are listening.
  auto event = std::make_unique<extensions::Event>(
      extensions::events::INPUT_METHOD_PRIVATE_ON_IME_MENU_LIST_CHANGED,
      OnImeMenuListChanged::kEventName, base::Value::List(), context_);
  router->BroadcastEvent(std::move(event));
}

void ExtensionImeMenuEventRouter::ImeMenuItemsChanged(
    const std::string& engine_id,
    const std::vector<input_method::InputMethodManager::MenuItem>& items) {
  extensions::EventRouter* router = extensions::EventRouter::Get(context_);

  if (!router->HasEventListener(OnImeMenuItemsChanged::kEventName))
    return;

  std::vector<input_method_private::MenuItem> menu_items;
  for (const auto& item : items) {
    input_method_private::MenuItem menu_item;
    menu_item.id = item.id;
    menu_item.label = item.label;
    switch (item.style) {
      case input_method::InputMethodManager::MENU_ITEM_STYLE_CHECK:
        menu_item.style = input_method_private::ParseMenuItemStyle("check");
        break;
      case input_method::InputMethodManager::MENU_ITEM_STYLE_RADIO:
        menu_item.style = input_method_private::ParseMenuItemStyle("radio");
        break;
      case input_method::InputMethodManager::MENU_ITEM_STYLE_SEPARATOR:
        menu_item.style = input_method_private::ParseMenuItemStyle("separator");
        break;
      default:
        menu_item.style = input_method_private::ParseMenuItemStyle("");
    }
    menu_item.visible = item.visible;
    menu_item.checked = item.checked;
    menu_item.enabled = item.enabled;
    menu_items.push_back(std::move(menu_item));
  }

  auto args = OnImeMenuItemsChanged::Create(engine_id, menu_items);

  // The router will only send the event to extensions that are listening.
  auto event = std::make_unique<extensions::Event>(
      extensions::events::INPUT_METHOD_PRIVATE_ON_IME_MENU_ITEMS_CHANGED,
      OnImeMenuItemsChanged::kEventName, std::move(args), context_);
  router->BroadcastEvent(std::move(event));
}

}  // namespace chromeos
