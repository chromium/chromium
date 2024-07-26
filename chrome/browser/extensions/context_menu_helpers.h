// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Definition of helper functions for the ContextMenus API.

#ifndef CHROME_BROWSER_EXTENSIONS_CONTEXT_MENU_HELPERS_H_
#define CHROME_BROWSER_EXTENSIONS_CONTEXT_MENU_HELPERS_H_

#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/types/optional_util.h"
#include "chrome/browser/extensions/menu_manager.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "content/public/browser/browser_context.h"
#include "extensions/common/error_utils.h"
#include "extensions/common/extension_id.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "extensions/common/utils/extension_utils.h"

namespace extensions {
namespace context_menu_helpers {

namespace {

template <typename PropertyWithEnumT>
std::unique_ptr<extensions::MenuItem::Id> GetParentId(
    const PropertyWithEnumT& property,
    bool is_off_the_record,
    const MenuItem::ExtensionKey& key) {
  if (!property.parent_id)
    return nullptr;

  std::unique_ptr<extensions::MenuItem::Id> parent_id(
      new extensions::MenuItem::Id(is_off_the_record, key));
  if (property.parent_id->as_integer)
    parent_id->uid = *property.parent_id->as_integer;
  else if (property.parent_id->as_string)
    parent_id->string_uid = *property.parent_id->as_string;
  else
    NOTREACHED_IN_MIGRATION();
  return parent_id;
}

}  // namespace

extern const char kActionNotAllowedError[];
extern const char kCannotFindItemError[];
extern const char kCheckedError[];
extern const char kDuplicateIDError[];
extern const char kGeneratedIdKey[];
extern const char kLauncherNotAllowedError[];
extern const char kOnclickDisallowedError[];
extern const char kParentsMustBeNormalError[];
extern const char kTitleNeededError[];
extern const char kTooManyMenuItems[];

std::string GetIDString(const MenuItem::Id& id);

MenuItem* GetParent(MenuItem::Id parent_id,
                    const MenuManager* menu_manager,
                    std::string* error);

MenuItem::ContextList GetContexts(const std::vector<
    extensions::api::context_menus::ContextType>& in_contexts);

MenuItem::Type GetType(extensions::api::context_menus::ItemType type,
                       MenuItem::Type default_type);

// Creates and adds a menu item from |create_properties|.
template <typename PropertyWithEnumT>
bool CreateMenuItem(const PropertyWithEnumT& create_properties,
                    content::BrowserContext* browser_context,
                    const Extension* extension,
                    const MenuItem::Id& item_id,
                    std::string* error) {
  bool is_webview = item_id.extension_key.webview_instance_id != 0;
  MenuManager* menu_manager = MenuManager::Get(browser_context);

  if (menu_manager->MenuItemsSize(item_id.extension_key) >=
      MenuManager::kMaxItemsPerExtension) {
    *error = ErrorUtils::FormatErrorMessage(
        kTooManyMenuItems,
        base::NumberToString(MenuManager::kMaxItemsPerExtension));
    return false;
  }

  if (menu_manager->GetItemById(item_id)) {
    *error = ErrorUtils::FormatErrorMessage(kDuplicateIDError,
                                            GetIDString(item_id));
    return false;
  }

  if (!is_webview && BackgroundInfo::HasLazyContext(extension) &&
      create_properties.onclick) {
    *error = kOnclickDisallowedError;
    return false;
  }

  // Contexts.
  MenuItem::ContextList contexts;
  if (create_properties.contexts)
    contexts = GetContexts(*create_properties.contexts);
  else
    contexts.Add(MenuItem::PAGE);

  if (contexts.Contains(MenuItem::LAUNCHER)) {
    // Launcher item is not allowed for <webview>.
    if (is_webview || !extension->is_platform_app()) {
      *error = kLauncherNotAllowedError;
      return false;
    }
  }

  if (contexts.Contains(MenuItem::BROWSER_ACTION) ||
      contexts.Contains(MenuItem::PAGE_ACTION) ||
      contexts.Contains(MenuItem::ACTION)) {
    // Action items are not allowed for <webview>.
    if (is_webview || !extension->is_extension()) {
      *error = kActionNotAllowedError;
      return false;
    }
  }

  // Title.
  std::string title;
  if (create_properties.title)
    title = *create_properties.title;

  MenuItem::Type type = GetType(create_properties.type, MenuItem::NORMAL);
  if (title.empty() && type != MenuItem::SEPARATOR) {
    *error = kTitleNeededError;
    return false;
  }

  // Visibility state.
  bool visible = create_properties.visible.value_or(true);

  // Checked state.
  bool checked = create_properties.checked.value_or(false);

  // Enabled.
  bool enabled = create_properties.enabled.value_or(true);

  std::unique_ptr<MenuItem> item(
      new MenuItem(item_id, title, checked, visible, enabled, type, contexts));

  // URL Patterns.
  if (!item->PopulateURLPatterns(
          base::OptionalToPtr(create_properties.document_url_patterns),
          base::OptionalToPtr(create_properties.target_url_patterns), error)) {
    return false;
  }

  // Parent id.
  bool success = true;
  std::unique_ptr<MenuItem::Id> parent_id(
      GetParentId(create_properties, browser_context->IsOffTheRecord(),
                  item_id.extension_key));
  if (parent_id.get()) {
    MenuItem* parent = GetParent(*parent_id, menu_manager, error);
    if (!parent)
      return false;
    success = menu_manager->AddChildItem(parent->id(), std::move(item));
  } else {
    success = menu_manager->AddContextItem(extension, std::move(item));
  }

  if (!success)
    return false;

  menu_manager->WriteToStorage(extension, item_id.extension_key);
  return true;
}

// Updates a menu item from |update_properties|.
template <typename PropertyWithEnumT>
bool UpdateMenuItem(const PropertyWithEnumT& update_properties,
                    content::BrowserContext* browser_context,
                    const Extension* extension,
                    const MenuItem::Id& item_id,
                    std::string* error) {
  bool radio_item_updated = false;
  bool is_webview = item_id.extension_key.webview_instance_id != 0;
  MenuManager* menu_manager = MenuManager::Get(browser_context);

  MenuItem* item = menu_manager->GetItemById(item_id);
  const ExtensionId& extension_id = MaybeGetExtensionId(extension);
  if (!item || item->extension_id() != extension_id) {
    *error = ErrorUtils::FormatErrorMessage(
        kCannotFindItemError, GetIDString(item_id));
    return false;
  }

  // Type.
  MenuItem::Type type = GetType(update_properties.type, item->type());

  if (type != item->type()) {
    if (type == MenuItem::RADIO || item->type() == MenuItem::RADIO)
      radio_item_updated = true;
    item->set_type(type);
  }

  // Title.
  if (update_properties.title) {
    std::string title(*update_properties.title);
    if (title.empty() && item->type() != MenuItem::SEPARATOR) {
      *error = kTitleNeededError;
      return false;
    }
    item->set_title(title);
  }

  // Checked state.
  if (update_properties.checked) {
    bool checked = *update_properties.checked;
    if (checked &&
        item->type() != MenuItem::CHECKBOX &&
        item->type() != MenuItem::RADIO) {
      *error = kCheckedError;
      return false;
    }

    const bool should_toggle_checked =
        // If radio item was unchecked nothing should happen. The radio item
        // should remain checked because there should always be one item checked
        // in the radio list.
        (item->type() == MenuItem::RADIO && checked) ||
        // Checkboxes are always updated.
        item->type() == MenuItem::CHECKBOX;

    if (should_toggle_checked) {
      if (!item->SetChecked(checked)) {
        *error = kCheckedError;
        return false;
      }
      radio_item_updated = true;
    }
  }

  // Visibility state.
  if (update_properties.visible)
    item->set_visible(*update_properties.visible);

  // Enabled.
  if (update_properties.enabled)
    item->set_enabled(*update_properties.enabled);

  // Contexts.
  MenuItem::ContextList contexts;
  if (update_properties.contexts) {
    contexts = GetContexts(*update_properties.contexts);

    if (contexts.Contains(MenuItem::LAUNCHER)) {
      // Launcher item is not allowed for <webview>.
      if (is_webview || !extension->is_platform_app()) {
        *error = kLauncherNotAllowedError;
        return false;
      }
    }

    if (contexts != item->contexts())
      item->set_contexts(contexts);
  }

  // Parent id.
  std::unique_ptr<MenuItem::Id> parent_id(
      GetParentId(update_properties, browser_context->IsOffTheRecord(),
                  item_id.extension_key));
  if (parent_id.get()) {
    MenuItem* parent = GetParent(*parent_id, menu_manager, error);
    if (!parent || !menu_manager->ChangeParent(item->id(), &parent->id()))
      return false;
  }

  // URL Patterns.
  if (!item->PopulateURLPatterns(
          base::OptionalToPtr(update_properties.document_url_patterns),
          base::OptionalToPtr(update_properties.target_url_patterns), error)) {
    return false;
  }

  // There is no need to call ItemUpdated if ChangeParent is called because
  // all sanitation is taken care of in ChangeParent.
  if (radio_item_updated && !menu_manager->ItemUpdated(item->id()))
    return false;

  menu_manager->WriteToStorage(extension, item_id.extension_key);
  return true;
}

}  // namespace context_menu_helpers
}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CONTEXT_MENU_HELPERS_H_
