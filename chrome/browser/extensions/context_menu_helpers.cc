// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/context_menu_helpers.h"

#include <stddef.h>

#include "base/strings/string_number_conversions.h"

namespace extensions {
namespace context_menu_helpers {

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
const char kTooManyMenuItems[] =
    "An extension can create a maximum of * menu items.";

std::string GetIDString(const MenuItem::Id& id) {
  if (id.uid == 0) {
    return id.string_uid;
  } else {
    return base::NumberToString(id.uid);
  }
}

MenuItem* GetParent(MenuItem::Id parent_id,
                    const MenuManager* menu_manager,
                    std::string* error) {
  MenuItem* parent = menu_manager->GetItemById(parent_id);
  if (!parent) {
    *error = ErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(parent_id));
    return nullptr;
  }
  if (parent->type() != MenuItem::NORMAL) {
    *error = kParentsMustBeNormalError;
    return nullptr;
  }
  return parent;
}

MenuItem::ContextList GetContexts(const std::vector<
    extensions::api::context_menus::ContextType>& in_contexts) {
  MenuItem::ContextList contexts;
  for (auto context : in_contexts) {
    switch (context) {
      case extensions::api::context_menus::ContextType::kAll:
        contexts.Add(extensions::MenuItem::ALL);
        break;
      case extensions::api::context_menus::ContextType::kPage:
        contexts.Add(extensions::MenuItem::PAGE);
        break;
      case extensions::api::context_menus::ContextType::kSelection:
        contexts.Add(extensions::MenuItem::SELECTION);
        break;
      case extensions::api::context_menus::ContextType::kLink:
        contexts.Add(extensions::MenuItem::LINK);
        break;
      case extensions::api::context_menus::ContextType::kEditable:
        contexts.Add(extensions::MenuItem::EDITABLE);
        break;
      case extensions::api::context_menus::ContextType::kImage:
        contexts.Add(extensions::MenuItem::IMAGE);
        break;
      case extensions::api::context_menus::ContextType::kVideo:
        contexts.Add(extensions::MenuItem::VIDEO);
        break;
      case extensions::api::context_menus::ContextType::kAudio:
        contexts.Add(extensions::MenuItem::AUDIO);
        break;
      case extensions::api::context_menus::ContextType::kFrame:
        contexts.Add(extensions::MenuItem::FRAME);
        break;
      case extensions::api::context_menus::ContextType::kLauncher:
        // Not available for <webview>.
        contexts.Add(extensions::MenuItem::LAUNCHER);
        break;
      case extensions::api::context_menus::ContextType::kBrowserAction:
        // Not available for <webview>.
        contexts.Add(extensions::MenuItem::BROWSER_ACTION);
        break;
      case extensions::api::context_menus::ContextType::kPageAction:
        // Not available for <webview>.
        contexts.Add(extensions::MenuItem::PAGE_ACTION);
        break;
      case extensions::api::context_menus::ContextType::kAction:
        // Not available for <webview>.
        contexts.Add(extensions::MenuItem::ACTION);
        break;
      case extensions::api::context_menus::ContextType::kNone:
        NOTREACHED_IN_MIGRATION();
    }
  }
  return contexts;
}

MenuItem::Type GetType(extensions::api::context_menus::ItemType type,
                       MenuItem::Type default_type) {
  switch (type) {
    case extensions::api::context_menus::ItemType::kNone:
      return default_type;
    case extensions::api::context_menus::ItemType::kNormal:
      return extensions::MenuItem::NORMAL;
    case extensions::api::context_menus::ItemType::kCheckbox:
      return extensions::MenuItem::CHECKBOX;
    case extensions::api::context_menus::ItemType::kRadio:
      return extensions::MenuItem::RADIO;
    case extensions::api::context_menus::ItemType::kSeparator:
      return extensions::MenuItem::SEPARATOR;
  }
  return extensions::MenuItem::NORMAL;
}

}  // namespace context_menu_helpers
}  // namespace extensions
