// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/context_menus/context_menus_api_helpers.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"

namespace extensions {
namespace context_menus_api_helpers {

const char kActionNotAllowedError[] =
    "Only extensions are allowed to use action contexts";
const char kCannotFindItemError[] = "Cannot find menu item with id *";
const char kCheckedError[] =
    "Only items with type \"radio\" or \"checkbox\" can be checked";
const char kDuplicateIDError[] =
    "Cannot create item with duplicate id *";
const char kGeneratedIdKey[] = "generatedId";
const char kLauncherNotAllowedError[] =
    "Only packaged apps are allowed to use 'launcher' context";
const char kOnclickDisallowedError[] =
    "Extensions using event pages or "
    "Service Workers cannot pass an onclick parameter to "
    "chrome.contextMenus.create. Instead, use the "
    "chrome.contextMenus.onClicked event.";
const char kParentsMustBeNormalError[] =
    "Parent items must have type \"normal\"";
const char kTitleNeededError[] =
    "All menu items except for separators must have a title";


std::string GetIDString(const MenuItem::Id& id) {
  if (id.uid == 0)
    return id.string_uid;
  else
    return base::NumberToString(id.uid);
}

MenuItem* GetParent(MenuItem::Id parent_id,
                    const MenuManager* menu_manager,
                    std::string* error) {
  MenuItem* parent = menu_manager->GetItemById(parent_id);
  if (!parent) {
    *error = ErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(parent_id));
    return NULL;
  }
  if (parent->type() != MenuItem::NORMAL) {
    *error = kParentsMustBeNormalError;
    return NULL;
  }
  return parent;
}

MenuItem::ContextList GetContexts(const std::vector<
    extensions::api::context_menus::ContextType>& in_contexts) {
  MenuItem::ContextList contexts;
  for (size_t i = 0; i < in_contexts.size(); ++i) {
    switch (in_contexts[i]) {
      case extensions::api::context_menus::CONTEXT_TYPE_ALL:
        contexts.Add(extensions::MenuItem::ALL);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_PAGE:
        contexts.Add(extensions::MenuItem::PAGE);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_SELECTION:
        contexts.Add(extensions::MenuItem::SELECTION);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_LINK:
        contexts.Add(extensions::MenuItem::LINK);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_EDITABLE:
        contexts.Add(extensions::MenuItem::EDITABLE);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_IMAGE:
        contexts.Add(extensions::MenuItem::IMAGE);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_VIDEO:
        contexts.Add(extensions::MenuItem::VIDEO);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_AUDIO:
        contexts.Add(extensions::MenuItem::AUDIO);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_FRAME:
        contexts.Add(extensions::MenuItem::FRAME);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_LAUNCHER:
        // Not available for <webview>.
        contexts.Add(extensions::MenuItem::LAUNCHER);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_BROWSER_ACTION:
        // Not available for <webview>.
        contexts.Add(extensions::MenuItem::BROWSER_ACTION);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_PAGE_ACTION:
        // Not available for <webview>.
        contexts.Add(extensions::MenuItem::PAGE_ACTION);
        break;
      case extensions::api::context_menus::CONTEXT_TYPE_NONE:
        NOTREACHED();
    }
  }
  return contexts;
}

MenuItem::Type GetType(extensions::api::context_menus::ItemType type,
                       MenuItem::Type default_type) {
  switch (type) {
    case extensions::api::context_menus::ITEM_TYPE_NONE:
      return default_type;
    case extensions::api::context_menus::ITEM_TYPE_NORMAL:
      return extensions::MenuItem::NORMAL;
    case extensions::api::context_menus::ITEM_TYPE_CHECKBOX:
      return extensions::MenuItem::CHECKBOX;
    case extensions::api::context_menus::ITEM_TYPE_RADIO:
      return extensions::MenuItem::RADIO;
    case extensions::api::context_menus::ITEM_TYPE_SEPARATOR:
      return extensions::MenuItem::SEPARATOR;
  }
  return extensions::MenuItem::NORMAL;
}

bool HasLazyContext(const Extension* extension) {
  return BackgroundInfo::HasLazyBackgroundPage(extension) ||
         BackgroundInfo::IsServiceWorkerBased(extension);
}

}  // namespace context_menus_api_helpers
}  // namespace extensions
