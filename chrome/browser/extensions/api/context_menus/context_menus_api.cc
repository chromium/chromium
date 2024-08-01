// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/context_menus/context_menus_api.h"

#include <string>
#include <utility>

#include "base/strings/string_util.h"
#include "base/values.h"
#include "chrome/browser/extensions/context_menu_helpers.h"
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
  std::optional<api::context_menus::Create::Params> params =
      api::context_menus::Create::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->create_properties.id) {
    id.string_uid = *params->create_properties.id;
  } else {
    if (BackgroundInfo::HasLazyContext(extension()))
      return RespondNow(Error(kIdRequiredError));

    // The Generated Id is added by context_menus_custom_bindings.js.
    EXTENSION_FUNCTION_VALIDATE(args().size() >= 1);
    EXTENSION_FUNCTION_VALIDATE(args()[0].is_dict());

    const base::Value& properties = args()[0];
    std::optional<int> result =
        properties.GetDict().FindInt(context_menu_helpers::kGeneratedIdKey);
    EXTENSION_FUNCTION_VALIDATE(result);
    id.uid = *result;
  }

  std::string error;
  if (!context_menu_helpers::CreateMenuItem(params->create_properties,
                                            browser_context(), extension(), id,
                                            &error)) {
    return RespondNow(Error(std::move(error)));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction ContextMenusUpdateFunction::Run() {
  MenuItem::Id item_id(browser_context()->IsOffTheRecord(),
                       MenuItem::ExtensionKey(extension_id()));
  std::optional<api::context_menus::Update::Params> params =
      api::context_menus::Update::Params::Create(args());

  EXTENSION_FUNCTION_VALIDATE(params);
  if (params->id.as_string)
    item_id.string_uid = *params->id.as_string;
  else if (params->id.as_integer)
    item_id.uid = *params->id.as_integer;
  else
    NOTREACHED_IN_MIGRATION();

  std::string error;
  if (!context_menu_helpers::UpdateMenuItem(params->update_properties,
                                            browser_context(), extension(),
                                            item_id, &error)) {
    return RespondNow(Error(std::move(error)));
  }
  return RespondNow(NoArguments());
}

ExtensionFunction::ResponseAction ContextMenusRemoveFunction::Run() {
  std::optional<api::context_menus::Remove::Params> params =
      api::context_menus::Remove::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  MenuManager* manager = MenuManager::Get(browser_context());

  MenuItem::Id id(browser_context()->IsOffTheRecord(),
                  MenuItem::ExtensionKey(extension_id()));
  if (params->menu_item_id.as_string)
    id.string_uid = *params->menu_item_id.as_string;
  else if (params->menu_item_id.as_integer)
    id.uid = *params->menu_item_id.as_integer;
  else
    NOTREACHED_IN_MIGRATION();

  MenuItem* item = manager->GetItemById(id);
  // Ensure one extension can't remove another's menu items.
  if (!item || item->extension_id() != extension_id()) {
    return RespondNow(Error(context_menu_helpers::kCannotFindItemError,
                            context_menu_helpers::GetIDString(id)));
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
