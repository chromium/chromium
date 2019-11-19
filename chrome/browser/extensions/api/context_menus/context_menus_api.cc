// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/context_menus/context_menus_api.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/context_menus/context_menus_api_helpers.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/url_pattern_set.h"

using extensions::ErrorUtils;

namespace {

const char kIdRequiredError[] =
    "Extensions using event pages or Service "
    "Workers must pass an id parameter to chrome.contextMenus.create";

}  // namespace

namespace extensions {

ExtensionFunction::ResponseAction ContextMenusCreateFunction::Run() {
  MenuItem::Id id(browser_context()->IsOffTheRecord(),
                  MenuItem::ExtensionKey(extension_id()));
  std::unique_ptr<api::context_menus::Create::Params> params(
      api::context_menus::Create::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  if (params->create_properties.id.get()) {
    id.string_uid = *params->create_properties.id;
  } else {
    if (context_menus_api_helpers::HasLazyContext(extension()))
      return RespondNow(Error(kIdRequiredError));

    // The Generated Id is added by context_menus_custom_bindings.js.
    base::DictionaryValue* properties = NULL;
    EXTENSION_FUNCTION_VALIDATE(args_->GetDictionary(0, &properties));
    EXTENSION_FUNCTION_VALIDATE(properties->GetInteger(
        extensions::context_menus_api_helpers::kGeneratedIdKey, &id.uid));
  }

  std::string error;
  if (!extensions::context_menus_api_helpers::CreateMenuItem(
          params->create_properties, browser_context(), extension(), id,
          &error)) {
    return RespondNow(Error(error));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction ContextMenusUpdateFunction::Run() {
  MenuItem::Id item_id(browser_context()->IsOffTheRecord(),
                       MenuItem::ExtensionKey(extension_id()));
  std::unique_ptr<api::context_menus::Update::Params> params(
      api::context_menus::Update::Params::Create(*args_));

  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->id.as_string)
    item_id.string_uid = *params->id.as_string;
  else if (params->id.as_integer)
    item_id.uid = *params->id.as_integer;
  else
    NOTREACHED();

  std::string error;
  if (!extensions::context_menus_api_helpers::UpdateMenuItem(
          params->update_properties, browser_context(), extension(), item_id,
          &error)) {
    return RespondNow(Error(error));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction ContextMenusRemoveFunction::Run() {
  std::unique_ptr<api::context_menus::Remove::Params> params(
      api::context_menus::Remove::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());

  MenuManager* manager = MenuManager::Get(browser_context());

  MenuItem::Id id(browser_context()->IsOffTheRecord(),
                  MenuItem::ExtensionKey(extension_id()));
  if (params->menu_item_id.as_string)
    id.string_uid = *params->menu_item_id.as_string;
  else if (params->menu_item_id.as_integer)
    id.uid = *params->menu_item_id.as_integer;
  else
    NOTREACHED();

  MenuItem* item = manager->GetItemById(id);
  // Ensure one extension can't remove another's menu items.
  if (!item || item->extension_id() != extension_id()) {
    return RespondNow(
        Error(extensions::context_menus_api_helpers::kCannotFindItemError,
              extensions::context_menus_api_helpers::GetIDString(id)));
  }

  if (!manager->RemoveContextMenuItem(id))
    return RespondNow(Error("Cannot remove menu item."));
  manager->WriteToStorage(extension(), id.extension_key);
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction ContextMenusRemoveAllFunction::Run() {
  MenuManager* manager = MenuManager::Get(browser_context());
  manager->RemoveAllContextItems(MenuItem::ExtensionKey(extension()->id()));
  manager->WriteToStorage(extension(),
                          MenuItem::ExtensionKey(extension()->id()));
  return RespondNow(NoArguments());
}

}  // namespace extensions
