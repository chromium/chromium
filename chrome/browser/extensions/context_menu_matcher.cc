// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/context_menu_matcher.h"

#include <string>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "content/public/browser/browser_context.h"
#include "content/public/common/context_menu_params.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/extension_util.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/image/image.h"

namespace extensions {

namespace {

// The range of command IDs reserved for extension's custom menus.
// TODO(oshima): These values will be injected by embedders.
int extensions_context_custom_first = IDC_EXTENSIONS_CONTEXT_CUSTOM_FIRST;
int extensions_context_custom_last = IDC_EXTENSIONS_CONTEXT_CUSTOM_LAST;

}  // namespace

// static
const size_t ContextMenuMatcher::kMaxExtensionItemTitleLength = 75;

// static
int ContextMenuMatcher::ConvertToExtensionsCustomCommandId(int id) {
  return extensions_context_custom_first + id;
}

// static
bool ContextMenuMatcher::IsExtensionsCustomCommandId(int id) {
  return id >= extensions_context_custom_first &&
         id <= extensions_context_custom_last;
}

ContextMenuMatcher::ContextMenuMatcher(
    content::BrowserContext* browser_context,
    ui::SimpleMenuModel::Delegate* delegate,
    ui::SimpleMenuModel* menu_model,
    const base::Callback<bool(const MenuItem*)>& filter)
    : browser_context_(browser_context),
      menu_model_(menu_model),
      delegate_(delegate),
      filter_(filter),
      is_smart_text_selection_enabled_(false) {}

void ContextMenuMatcher::AppendExtensionItems(
    const MenuItem::ExtensionKey& extension_key,
    const base::string16& selection_text,
    int* index,
    bool is_action_menu) {
  DCHECK_GE(*index, 0);
  int max_index =
      extensions_context_custom_last - extensions_context_custom_first;
  if (*index >= max_index)
    return;

  const Extension* extension = NULL;
  MenuItem::List items;
  bool can_cross_incognito;
  if (!GetRelevantExtensionTopLevelItems(
          extension_key, &extension, &can_cross_incognito, &items))
    return;

  if (items.empty())
    return;

  bool prepend_separator = false;

#if !defined(OS_CHROMEOS)
  // If this is the first extension-provided menu item, and there are other
  // items in the menu, and the last item is not a separator add a separator.
  // Also, don't add separators when Smart Text Selection is enabled. Smart
  // actions are grouped with extensions and the separator logic is
  // handled by them.
  prepend_separator = *index == 0 && menu_model_->GetItemCount() &&
                      !is_smart_text_selection_enabled_;
#endif

  // Extensions (other than platform apps) are only allowed one top-level slot
  // (and it can't be a radio or checkbox item because we are going to put the
  // extension icon next to it), unless the context menu is an an action menu.
  // Action menus do not include the extension action, and they only include
  // items from one extension, so they are not placed within a submenu.
  // Otherwise, we automatically push them into a submenu if there is more than
  // one top-level item.
  if (extension->is_platform_app() || is_action_menu) {
    if (prepend_separator)
      menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
    RecursivelyAppendExtensionItems(items,
                                    can_cross_incognito,
                                    selection_text,
                                    menu_model_,
                                    index,
                                    is_action_menu);
  } else {
    int menu_id = ConvertToExtensionsCustomCommandId(*index);
    (*index)++;
    base::string16 title;
    MenuItem::List submenu_items;

    if (items.size() > 1 || items[0]->type() != MenuItem::NORMAL) {
      if (prepend_separator)
        menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      title = base::UTF8ToUTF16(extension->name());
      submenu_items = items;
    } else {
      // The top-level menu item, |item[0]|, is sandwiched between two menu
      // separators. If the top-level menu item is visible, its preceding
      // separator should be included in the UI model, so that both separators
      // are shown. Otherwise if the top-level menu item is hidden, the
      // preceding separator should be excluded, so that only one of the two
      // separators remain.
      if (prepend_separator && items[0]->visible())
        menu_model_->AddSeparator(ui::NORMAL_SEPARATOR);
      MenuItem* item = items[0];
      extension_item_map_[menu_id] = item->id();
      title = item->TitleWithReplacement(selection_text,
                                       kMaxExtensionItemTitleLength);
      submenu_items = GetRelevantExtensionItems(item->children(),
                                                can_cross_incognito);
    }

    // Now add our item(s) to the menu_model_.
    if (submenu_items.empty()) {
      menu_model_->AddItem(menu_id, title);
    } else {
      ui::SimpleMenuModel* submenu = new ui::SimpleMenuModel(delegate_);
      extension_menu_models_.push_back(base::WrapUnique(submenu));
      menu_model_->AddSubMenu(menu_id, title, submenu);
      RecursivelyAppendExtensionItems(submenu_items, can_cross_incognito,
                                      selection_text, submenu, index,
                                      false);  // is_action_menu_top_level
    }
    if (!is_action_menu)
      SetExtensionIcon(extension_key.extension_id);
  }
}

void ContextMenuMatcher::Clear() {
  extension_item_map_.clear();
  extension_menu_models_.clear();
}

base::string16 ContextMenuMatcher::GetTopLevelContextMenuTitle(
    const MenuItem::ExtensionKey& extension_key,
    const base::string16& selection_text) {
  const Extension* extension = NULL;
  MenuItem::List items;
  bool can_cross_incognito;
  GetRelevantExtensionTopLevelItems(
      extension_key, &extension, &can_cross_incognito, &items);

  base::string16 title;

  if (items.empty() ||
      items.size() > 1 ||
      items[0]->type() != MenuItem::NORMAL) {
    title = base::UTF8ToUTF16(extension->name());
  } else {
    MenuItem* item = items[0];
    title = item->TitleWithReplacement(
        selection_text, kMaxExtensionItemTitleLength);
  }
  return title;
}

bool ContextMenuMatcher::IsCommandIdChecked(int command_id) const {
  MenuItem* item = GetExtensionMenuItem(command_id);
  if (!item)
    return false;
  return item->checked();
}

bool ContextMenuMatcher::IsCommandIdVisible(int command_id) const {
  MenuItem* item = GetExtensionMenuItem(command_id);
  // The context menu code creates a top-level menu item, labeled with the
  // extension's name, that is a container of an extension's menu items. This
  // top-level menu item is not added to the context menu, so checking its
  // visibility is a special case handled below. This top-level menu item should
  // always be displayed.
  if (!item && ContextMenuMatcher::IsExtensionsCustomCommandId(command_id)) {
    return true;
  } else if (item) {
    return item->visible();
  } else {
    return false;
  }
}

bool ContextMenuMatcher::IsCommandIdEnabled(int command_id) const {
  MenuItem* item = GetExtensionMenuItem(command_id);
  if (!item)
    return true;
  return item->enabled();
}

void ContextMenuMatcher::ExecuteCommand(
    int command_id,
    content::WebContents* web_contents,
    content::RenderFrameHost* render_frame_host,
    const content::ContextMenuParams& params) {
  MenuItem* item = GetExtensionMenuItem(command_id);
  if (!item)
    return;

  MenuManager* manager = MenuManager::Get(browser_context_);
  manager->ExecuteCommand(browser_context_, web_contents, render_frame_host,
                          params, item->id());
}

bool ContextMenuMatcher::GetRelevantExtensionTopLevelItems(
    const MenuItem::ExtensionKey& extension_key,
    const Extension** extension,
    bool* can_cross_incognito,
    MenuItem::List* items) {
  *extension = ExtensionRegistry::Get(
      browser_context_)->enabled_extensions().GetByID(
          extension_key.extension_id);
  if (!*extension)
    return false;

  // Find matching items.
  MenuManager* manager = MenuManager::Get(browser_context_);
  const MenuItem::OwnedList* all_items = manager->MenuItems(extension_key);
  if (!all_items || all_items->empty())
    return false;

  *can_cross_incognito = util::CanCrossIncognito(*extension, browser_context_);
  *items = GetRelevantExtensionItems(*all_items, *can_cross_incognito);

  return true;
}

MenuItem::List ContextMenuMatcher::GetRelevantExtensionItems(
    const MenuItem::OwnedList& items,
    bool can_cross_incognito) {
  MenuItem::List result;
  for (auto i = items.begin(); i != items.end(); ++i) {
    MenuItem* item = i->get();

    if (!filter_.Run(item))
      continue;

    if (item->id().incognito == browser_context_->IsOffTheRecord() ||
        can_cross_incognito)
      result.push_back(item);
  }
  return result;
}

void ContextMenuMatcher::RecursivelyAppendExtensionItems(
    const MenuItem::List& items,
    bool can_cross_incognito,
    const base::string16& selection_text,
    ui::SimpleMenuModel* menu_model,
    int* index,
    bool is_action_menu_top_level) {
  MenuItem::Type last_type = MenuItem::NORMAL;
  int radio_group_id = 1;
  int num_visible_items = 0;

  bool enable_separators = false;

#if !defined(OS_CHROMEOS)
  enable_separators = true;
#endif

  for (auto i = items.begin(); i != items.end(); ++i) {
    MenuItem* item = *i;

    // If last item was of type radio but the current one isn't, auto-insert
    // a separator.  The converse case is handled below.
    if (last_type == MenuItem::RADIO && item->type() != MenuItem::RADIO &&
        enable_separators) {
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
      last_type = MenuItem::SEPARATOR;
    }

    int menu_id = ConvertToExtensionsCustomCommandId(*index);
    // Action context menus have a limit for top level extension items to
    // prevent control items from being pushed off the screen, since extension
    // items will not be placed in a submenu.
    const int top_level_limit = api::context_menus::ACTION_MENU_TOP_LEVEL_LIMIT;
    if (menu_id >= extensions_context_custom_last ||
        (is_action_menu_top_level && num_visible_items >= top_level_limit))
      return;

    ++(*index);
    if (item->visible())
      ++num_visible_items;

    extension_item_map_[menu_id] = item->id();
    base::string16 title = item->TitleWithReplacement(selection_text,
                                                kMaxExtensionItemTitleLength);
    if (item->type() == MenuItem::NORMAL) {
      MenuItem::List children =
          GetRelevantExtensionItems(item->children(), can_cross_incognito);
      if (children.empty()) {
        menu_model->AddItem(menu_id, title);
      } else {
        ui::SimpleMenuModel* submenu = new ui::SimpleMenuModel(delegate_);
        extension_menu_models_.push_back(base::WrapUnique(submenu));
        menu_model->AddSubMenu(menu_id, title, submenu);
        RecursivelyAppendExtensionItems(children, can_cross_incognito,
                                        selection_text, submenu, index,
                                        false);  // is_action_menu_top_level
      }
    } else if (item->type() == MenuItem::CHECKBOX) {
      menu_model->AddCheckItem(menu_id, title);
    } else if (item->type() == MenuItem::RADIO) {
      if (i != items.begin() &&
          last_type != MenuItem::RADIO) {
        radio_group_id++;

        // Auto-append a separator if needed.
        if (enable_separators)
          menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
      }

      menu_model->AddRadioItem(menu_id, title, radio_group_id);
    } else if (item->type() == MenuItem::SEPARATOR && enable_separators) {
      menu_model->AddSeparator(ui::NORMAL_SEPARATOR);
    }
    last_type = item->type();
  }
}

MenuItem* ContextMenuMatcher::GetExtensionMenuItem(int id) const {
  MenuManager* manager = MenuManager::Get(browser_context_);
  auto i = extension_item_map_.find(id);
  if (i != extension_item_map_.end()) {
    MenuItem* item = manager->GetItemById(i->second);
    if (item)
      return item;
  }
  return NULL;
}

void ContextMenuMatcher::SetExtensionIcon(const std::string& extension_id) {
  MenuManager* menu_manager = MenuManager::Get(browser_context_);

  int index = menu_model_->GetItemCount() - 1;
  DCHECK_GE(index, 0);

  gfx::Image icon = menu_manager->GetIconForExtension(extension_id);
  DCHECK_EQ(gfx::kFaviconSize, icon.Width());
  DCHECK_EQ(gfx::kFaviconSize, icon.Height());
  menu_model_->SetIcon(index, icon);
}

}  // namespace extensions
