// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/menu_manager.h"

#include <algorithm>
#include <memory>
#include <tuple>
#include <utility>

#include "base/bind.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/extensions/menu_manager_factory.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/chrome_web_view_internal.h"
#include "chrome/common/extensions/api/context_menus.h"
#include "chrome/common/extensions/api/url_handlers/url_handlers_parser.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/context_menu_params.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_api_frame_id_map.h"
#include "extensions/browser/guest_view/web_view/web_view_guest.h"
#include "extensions/browser/state_store.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_handlers/background_info.h"
#include "ui/gfx/favicon_size.h"
#include "ui/gfx/text_elider.h"

using content::ChildProcessHost;
using content::WebContents;
using guest_view::kInstanceIDNone;

namespace extensions {

namespace {

// Keys for serialization to and from Value to store in the preferences.
const char kContextMenusKey[] = "context_menus";

const char kCheckedKey[] = "checked";
const char kContextsKey[] = "contexts";
const char kDocumentURLPatternsKey[] = "document_url_patterns";
const char kEnabledKey[] = "enabled";
const char kMenuManagerIncognitoKey[] = "incognito";
const char kParentUIDKey[] = "parent_uid";
const char kStringUIDKey[] = "string_uid";
const char kTargetURLPatternsKey[] = "target_url_patterns";
const char kTitleKey[] = "title";
const char kMenuManagerTypeKey[] = "type";
const char kVisibleKey[] = "visible";

void SetIdKeyValue(base::DictionaryValue* properties,
                   const char* key,
                   const MenuItem::Id& id) {
  if (id.uid == 0)
    properties->SetString(key, id.string_uid);
  else
    properties->SetInteger(key, id.uid);
}

MenuItem::OwnedList MenuItemsFromValue(const std::string& extension_id,
                                       base::Value* value) {
  MenuItem::OwnedList items;

  base::ListValue* list = nullptr;
  if (!value || !value->GetAsList(&list))
    return items;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    base::DictionaryValue* dict = nullptr;
    if (!list->GetDictionary(i, &dict))
      continue;
    std::unique_ptr<MenuItem> item =
        MenuItem::Populate(extension_id, *dict, nullptr);
    if (!item)
      continue;
    items.push_back(std::move(item));
  }
  return items;
}

std::unique_ptr<base::ListValue> MenuItemsToValue(const MenuItem::List& items) {
  std::unique_ptr<base::ListValue> list(new base::ListValue());
  for (size_t i = 0; i < items.size(); ++i)
    list->Append(items[i]->ToValue());
  return list;
}

bool GetStringList(const base::DictionaryValue& dict,
                   const std::string& key,
                   std::vector<std::string>* out) {
  if (!dict.HasKey(key))
    return true;

  const base::ListValue* list = nullptr;
  if (!dict.GetListWithoutPathExpansion(key, &list))
    return false;

  for (size_t i = 0; i < list->GetSize(); ++i) {
    std::string pattern;
    if (!list->GetString(i, &pattern))
      return false;
    out->push_back(pattern);
  }

  return true;
}

}  // namespace

MenuItem::MenuItem(const Id& id,
                   const std::string& title,
                   bool checked,
                   bool visible,
                   bool enabled,
                   Type type,
                   const ContextList& contexts)
    : id_(id),
      title_(title),
      type_(type),
      checked_(checked),
      visible_(visible),
      enabled_(enabled),
      contexts_(contexts) {}

MenuItem::~MenuItem() {
}

std::unique_ptr<MenuItem> MenuItem::ReleaseChild(const Id& child_id,
                                                 bool recursive) {
  for (auto i = children_.begin(); i != children_.end(); ++i) {
    std::unique_ptr<MenuItem> child;
    if ((*i)->id() == child_id) {
      child = std::move(*i);
      children_.erase(i);
      return child;
    }
    if (recursive) {
      child = (*i)->ReleaseChild(child_id, recursive);
      if (child)
        return child;
    }
  }
  return nullptr;
}

void MenuItem::GetFlattenedSubtree(MenuItem::List* list) {
  list->push_back(this);
  for (const auto& child : children_)
    child->GetFlattenedSubtree(list);
}

std::set<MenuItem::Id> MenuItem::RemoveAllDescendants() {
  std::set<Id> result;
  for (const auto& child : children_) {
    result.insert(child->id());
    std::set<Id> removed = child->RemoveAllDescendants();
    result.insert(removed.begin(), removed.end());
  }
  children_.clear();
  return result;
}

base::string16 MenuItem::TitleWithReplacement(const base::string16& selection,
                                              size_t max_length) const {
  base::string16 result = base::UTF8ToUTF16(title_);
  // TODO(asargent) - Change this to properly handle %% escaping so you can
  // put "%s" in titles that won't get substituted.
  base::ReplaceSubstringsAfterOffset(
      &result, 0, base::ASCIIToUTF16("%s"), selection);

  if (result.length() > max_length)
    result = gfx::TruncateString(result, max_length, gfx::WORD_BREAK);
  return result;
}

bool MenuItem::SetChecked(bool checked) {
  if (type_ != CHECKBOX && type_ != RADIO)
    return false;
  checked_ = checked;
  return true;
}

void MenuItem::AddChild(std::unique_ptr<MenuItem> item) {
  item->parent_id_.reset(new Id(id_));
  children_.push_back(std::move(item));
}

std::unique_ptr<base::DictionaryValue> MenuItem::ToValue() const {
  std::unique_ptr<base::DictionaryValue> value(new base::DictionaryValue);
  // Should only be called for extensions with event pages, which only have
  // string IDs for items.
  DCHECK_EQ(0, id_.uid);
  value->SetString(kStringUIDKey, id_.string_uid);
  value->SetBoolean(kMenuManagerIncognitoKey, id_.incognito);
  value->SetInteger(kMenuManagerTypeKey, type_);
  if (type_ != SEPARATOR)
    value->SetString(kTitleKey, title_);
  if (type_ == CHECKBOX || type_ == RADIO)
    value->SetBoolean(kCheckedKey, checked_);
  value->SetBoolean(kEnabledKey, enabled_);
  value->SetBoolean(kVisibleKey, visible_);
  value->Set(kContextsKey, contexts_.ToValue());
  if (parent_id_) {
    DCHECK_EQ(0, parent_id_->uid);
    value->SetString(kParentUIDKey, parent_id_->string_uid);
  }
  value->Set(kDocumentURLPatternsKey, document_url_patterns_.ToValue());
  value->Set(kTargetURLPatternsKey, target_url_patterns_.ToValue());
  return value;
}

// static
std::unique_ptr<MenuItem> MenuItem::Populate(const std::string& extension_id,
                                             const base::DictionaryValue& value,
                                             std::string* error) {
  bool incognito = false;
  if (!value.GetBoolean(kMenuManagerIncognitoKey, &incognito))
    return nullptr;
  Id id(incognito, MenuItem::ExtensionKey(extension_id));
  if (!value.GetString(kStringUIDKey, &id.string_uid))
    return nullptr;
  int type_int;
  Type type = NORMAL;
  if (!value.GetInteger(kMenuManagerTypeKey, &type_int))
    return nullptr;
  type = static_cast<Type>(type_int);
  std::string title;
  if (type != SEPARATOR && !value.GetString(kTitleKey, &title))
    return nullptr;
  bool checked = false;
  if ((type == CHECKBOX || type == RADIO) &&
      !value.GetBoolean(kCheckedKey, &checked)) {
    return nullptr;
  }
  // The ability to toggle a menu item's visibility was introduced in M62, so it
  // is expected that the kVisibleKey will not be present in older menu items in
  // storage. Thus, we do not return nullptr if the kVisibleKey is not found.
  // TODO(catmullings): Remove this in M65 when all prefs should be migrated.
  bool visible = true;
  value.GetBoolean(kVisibleKey, &visible);
  bool enabled = true;
  if (!value.GetBoolean(kEnabledKey, &enabled))
    return nullptr;
  ContextList contexts;
  const base::Value* contexts_value = nullptr;
  if (!value.Get(kContextsKey, &contexts_value))
    return nullptr;
  if (!contexts.Populate(*contexts_value))
    return nullptr;

  std::unique_ptr<MenuItem> result = std::make_unique<MenuItem>(
      id, title, checked, visible, enabled, type, contexts);

  std::vector<std::string> document_url_patterns;
  if (!GetStringList(value, kDocumentURLPatternsKey, &document_url_patterns))
    return nullptr;
  std::vector<std::string> target_url_patterns;
  if (!GetStringList(value, kTargetURLPatternsKey, &target_url_patterns))
    return nullptr;

  if (!result->PopulateURLPatterns(&document_url_patterns,
                                   &target_url_patterns,
                                   error)) {
    return nullptr;
  }

  // parent_id is filled in from the value, but it might not be valid. It's left
  // to be validated upon being added (via AddChildItem) to the menu manager.
  std::unique_ptr<Id> parent_id =
      std::make_unique<Id>(incognito, MenuItem::ExtensionKey(extension_id));
  if (value.HasKey(kParentUIDKey)) {
    if (!value.GetString(kParentUIDKey, &parent_id->string_uid))
      return nullptr;
    result->parent_id_.swap(parent_id);
  }
  return result;
}

bool MenuItem::PopulateURLPatterns(
    std::vector<std::string>* document_url_patterns,
    std::vector<std::string>* target_url_patterns,
    std::string* error) {
  if (document_url_patterns) {
    if (!document_url_patterns_.Populate(
            *document_url_patterns, URLPattern::SCHEME_ALL, true, error)) {
      return false;
    }
  }
  if (target_url_patterns) {
    if (!target_url_patterns_.Populate(
            *target_url_patterns, URLPattern::SCHEME_ALL, true, error)) {
      return false;
    }
  }
  return true;
}

// static
const char MenuManager::kOnContextMenus[] = "contextMenus";
const char MenuManager::kOnWebviewContextMenus[] =
    "webViewInternal.contextMenus";

MenuManager::MenuManager(content::BrowserContext* context, StateStore* store)
    : browser_context_(context), store_(store) {
  extension_registry_observer_.Add(ExtensionRegistry::Get(browser_context_));
  Profile* profile = Profile::FromBrowserContext(context);
  observed_profiles_.Add(profile);
  if (profile->HasOffTheRecordProfile())
    observed_profiles_.Add(profile->GetOffTheRecordProfile());
  if (store_)
    store_->RegisterKey(kContextMenusKey);
}

MenuManager::~MenuManager() = default;

// static
MenuManager* MenuManager::Get(content::BrowserContext* context) {
  return MenuManagerFactory::GetForBrowserContext(context);
}

std::set<MenuItem::ExtensionKey> MenuManager::ExtensionIds() {
  std::set<MenuItem::ExtensionKey> id_set;
  for (auto i = context_items_.begin(); i != context_items_.end(); ++i) {
    id_set.insert(i->first);
  }
  return id_set;
}

const MenuItem::OwnedList* MenuManager::MenuItems(
    const MenuItem::ExtensionKey& key) {
  auto i = context_items_.find(key);
  if (i != context_items_.end()) {
    return &i->second;
  }
  return nullptr;
}

bool MenuManager::AddContextItem(const Extension* extension,
                                 std::unique_ptr<MenuItem> item) {
  MenuItem* item_ptr = item.get();
  const MenuItem::ExtensionKey& key = item->id().extension_key;

  // The item must have a non-empty key, and not have already been added.
  if (key.empty() || base::Contains(items_by_id_, item->id()))
    return false;

  DCHECK_EQ(extension->id(), key.extension_id);

  bool first_item = !base::Contains(context_items_, key);
  context_items_[key].push_back(std::move(item));
  items_by_id_[item_ptr->id()] = item_ptr;

  if (item_ptr->type() == MenuItem::RADIO) {
    if (item_ptr->checked())
      RadioItemSelected(item_ptr);
    else
      SanitizeRadioListsInMenu(context_items_[key]);
  }

  // If this is the first item for this extension, start loading its icon.
  if (first_item)
    icon_manager_.LoadIcon(browser_context_, extension);

  return true;
}

bool MenuManager::AddChildItem(const MenuItem::Id& parent_id,
                               std::unique_ptr<MenuItem> child) {
  MenuItem* parent = GetItemById(parent_id);
  if (!parent || parent->type() != MenuItem::NORMAL ||
      parent->incognito() != child->incognito() ||
      parent->extension_id() != child->extension_id() ||
      base::Contains(items_by_id_, child->id()))
    return false;
  MenuItem* child_ptr = child.get();
  parent->AddChild(std::move(child));
  items_by_id_[child_ptr->id()] = child_ptr;

  if (child_ptr->type() == MenuItem::RADIO)
    SanitizeRadioListsInMenu(parent->children());
  return true;
}

bool MenuManager::DescendantOf(MenuItem* item,
                               const MenuItem::Id& ancestor_id) {
  // Work our way up the tree until we find the ancestor or null.
  MenuItem::Id* id = item->parent_id();
  while (id != nullptr) {
    DCHECK(*id != item->id());  // Catch circular graphs.
    if (*id == ancestor_id)
      return true;
    MenuItem* next = GetItemById(*id);
    if (!next) {
      NOTREACHED();
      return false;
    }
    id = next->parent_id();
  }
  return false;
}

bool MenuManager::ChangeParent(const MenuItem::Id& child_id,
                               const MenuItem::Id* parent_id) {
  MenuItem* child_ptr = GetItemById(child_id);
  std::unique_ptr<MenuItem> child;

  MenuItem* new_parent = parent_id ? GetItemById(*parent_id) : nullptr;
  if ((parent_id && (child_id == *parent_id)) || !child_ptr ||
      (!new_parent && parent_id != nullptr) ||
      (new_parent && (DescendantOf(new_parent, child_id) ||
                      child_ptr->incognito() != new_parent->incognito() ||
                      child_ptr->extension_id() != new_parent->extension_id())))
    return false;

  MenuItem::Id* old_parent_id = child_ptr->parent_id();
  if (old_parent_id != nullptr) {
    MenuItem* old_parent = GetItemById(*old_parent_id);
    if (!old_parent) {
      NOTREACHED();
      return false;
    }
    child = old_parent->ReleaseChild(child_id, false /* non-recursive search*/);
    DCHECK(child.get() == child_ptr);
    SanitizeRadioListsInMenu(old_parent->children());
  } else {
    // This is a top-level item, so we need to pull it out of our list of
    // top-level items.
    const MenuItem::ExtensionKey& child_key = child_ptr->id().extension_key;
    auto i = context_items_.find(child_key);
    if (i == context_items_.end()) {
      NOTREACHED();
      return false;
    }
    MenuItem::OwnedList& list = i->second;
    auto j = std::find_if(list.begin(), list.end(),
                          [child_ptr](const std::unique_ptr<MenuItem>& item) {
                            return item.get() == child_ptr;
                          });
    if (j == list.end()) {
      NOTREACHED();
      return false;
    }
    child = std::move(*j);
    list.erase(j);
    SanitizeRadioListsInMenu(list);
  }

  if (new_parent) {
    new_parent->AddChild(std::move(child));
    SanitizeRadioListsInMenu(new_parent->children());
  } else {
    const MenuItem::ExtensionKey& child_key = child_ptr->id().extension_key;
    context_items_[child_key].push_back(std::move(child));
    child_ptr->parent_id_.reset(nullptr);
    SanitizeRadioListsInMenu(context_items_[child_key]);
  }
  return true;
}

bool MenuManager::RemoveContextMenuItem(const MenuItem::Id& id) {
  if (!base::Contains(items_by_id_, id))
    return false;

  MenuItem* menu_item = GetItemById(id);
  DCHECK(menu_item);
  const MenuItem::ExtensionKey extension_key = id.extension_key;
  auto i = context_items_.find(extension_key);
  if (i == context_items_.end()) {
    NOTREACHED();
    return false;
  }

  bool result = false;
  std::set<MenuItem::Id> items_removed;
  MenuItem::OwnedList& list = i->second;
  for (auto j = list.begin(); j < list.end(); ++j) {
    // See if the current top-level item is a match.
    if ((*j)->id() == id) {
      items_removed = (*j)->RemoveAllDescendants();
      items_removed.insert(id);
      list.erase(j);
      result = true;
      SanitizeRadioListsInMenu(list);
      break;
    } else {
      // See if the item to remove was found as a descendant of the current
      // top-level item.
      std::unique_ptr<MenuItem> child =
          (*j)->ReleaseChild(id, true /* recursive */);
      if (child) {
        items_removed = child->RemoveAllDescendants();
        items_removed.insert(id);
        SanitizeRadioListsInMenu(GetItemById(*child->parent_id())->children());
        result = true;
        break;
      }
    }
  }
  DCHECK(result);  // The check at the very top should have prevented this.

  // Clear entries from the items_by_id_ map.
  for (auto removed_iter = items_removed.begin();
       removed_iter != items_removed.end(); ++removed_iter) {
    items_by_id_.erase(*removed_iter);
  }

  if (list.empty()) {
    context_items_.erase(extension_key);
    icon_manager_.RemoveIcon(extension_key.extension_id);
  }
  return result;
}

void MenuManager::RemoveAllContextItems(
    const MenuItem::ExtensionKey& extension_key) {
  auto it = context_items_.find(extension_key);
  if (it == context_items_.end())
    return;

  // We use the |extension_id| from the stored ExtensionKey, since the provided
  // |extension_key| may leave it empty (if matching solely based on the
  // webview IDs).
  // TODO(paulmeyer): We can get rid of this hack if/when we reliably track
  // extension IDs at WebView cleanup.
  std::string extension_id = it->first.extension_id;
  MenuItem::OwnedList& context_items_for_key = it->second;
  for (const auto& item : context_items_for_key) {
    items_by_id_.erase(item->id());

    // Remove descendants from this item and erase them from the lookup cache.
    std::set<MenuItem::Id> removed_ids = item->RemoveAllDescendants();
    for (auto j = removed_ids.begin(); j != removed_ids.end(); ++j) {
      items_by_id_.erase(*j);
    }
  }
  context_items_.erase(extension_key);
  icon_manager_.RemoveIcon(extension_id);
}

MenuItem* MenuManager::GetItemById(const MenuItem::Id& id) const {
  auto i = items_by_id_.find(id);
  return i != items_by_id_.end() ? i->second : nullptr;
}

void MenuManager::RadioItemSelected(MenuItem* item) {
  // If this is a child item, we need to get a handle to the list from its
  // parent. Otherwise get a handle to the top-level list.
  const MenuItem::OwnedList* list = nullptr;
  if (item->parent_id()) {
    MenuItem* parent = GetItemById(*item->parent_id());
    if (!parent) {
      NOTREACHED();
      return;
    }
    list = &(parent->children());
  } else {
    const MenuItem::ExtensionKey& key = item->id().extension_key;
    if (context_items_.find(key) == context_items_.end()) {
      NOTREACHED();
      return;
    }
    list = &context_items_[key];
  }

  // Find where |item| is in the list.
  MenuItem::OwnedList::const_iterator item_location;
  for (item_location = list->begin(); item_location != list->end();
       ++item_location) {
    if (item_location->get() == item)
      break;
  }
  if (item_location == list->end()) {
    NOTREACHED();  // We should have found the item.
    return;
  }

  // Iterate backwards from |item| and uncheck any adjacent radio items.
  MenuItem::OwnedList::const_iterator i;
  if (item_location != list->begin()) {
    i = item_location;
    do {
      --i;
      if ((*i)->type() != MenuItem::RADIO)
        break;
      (*i)->SetChecked(false);
    } while (i != list->begin());
  }

  // Now iterate forwards from |item| and uncheck any adjacent radio items.
  for (i = item_location + 1; i != list->end(); ++i) {
    if ((*i)->type() != MenuItem::RADIO)
      break;
    (*i)->SetChecked(false);
  }
}

static void AddURLProperty(base::DictionaryValue* dictionary,
                           const std::string& key, const GURL& url) {
  if (!url.is_empty())
    dictionary->SetString(key, url.possibly_invalid_spec());
}

void MenuManager::ExecuteCommand(content::BrowserContext* context,
                                 WebContents* web_contents,
                                 content::RenderFrameHost* render_frame_host,
                                 const content::ContextMenuParams& params,
                                 const MenuItem::Id& menu_item_id) {
  EventRouter* event_router = EventRouter::Get(context);
  if (!event_router)
    return;

  MenuItem* item = GetItemById(menu_item_id);
  if (!item)
    return;

  ExtensionRegistry* registry = ExtensionRegistry::Get(browser_context_);
  const Extension* extension =
      registry->enabled_extensions().GetByID(item->extension_id());

  if (item->type() == MenuItem::RADIO)
    RadioItemSelected(item);


  std::unique_ptr<base::DictionaryValue> properties(
      new base::DictionaryValue());
  SetIdKeyValue(properties.get(), "menuItemId", item->id());
  if (item->parent_id())
    SetIdKeyValue(properties.get(), "parentMenuItemId", *item->parent_id());

  switch (params.media_type) {
    case blink::ContextMenuDataMediaType::kImage:
      properties->SetString("mediaType", "image");
      break;
    case blink::ContextMenuDataMediaType::kVideo:
      properties->SetString("mediaType", "video");
      break;
    case blink::ContextMenuDataMediaType::kAudio:
      properties->SetString("mediaType", "audio");
      break;
    default:  {}  // Do nothing.
  }

  AddURLProperty(properties.get(), "linkUrl", params.unfiltered_link_url);
  AddURLProperty(properties.get(), "srcUrl", params.src_url);
  AddURLProperty(properties.get(), "pageUrl", params.page_url);
  AddURLProperty(properties.get(), "frameUrl", params.frame_url);

  if (params.selection_text.length() > 0)
    properties->SetString("selectionText", params.selection_text);

  properties->SetBoolean("editable", params.is_editable);

  WebViewGuest* webview_guest = WebViewGuest::FromWebContents(web_contents);
  if (webview_guest) {
    // This is used in web_view_internalcustom_bindings.js.
    // The property is not exposed to developer API.
    properties->SetInteger("webviewInstanceId",
                           webview_guest->view_instance_id());
  }

  auto args = std::make_unique<base::ListValue>();
  args->Reserve(2);
  args->Append(std::move(properties));
  // |properties| is invalidated at this time, which is why |args| needs to be
  // queried for the pointer. The obtained pointer is guaranteed to stay valid
  // even after further Appends, because enough storage was reserved above.
  base::DictionaryValue* raw_properties = nullptr;
  args->GetDictionary(0, &raw_properties);

  // Add the tab info to the argument list.
  // No tab info in a platform app.
  if (!extension || !extension->is_platform_app()) {
    // Note: web_contents are null in unit tests :(
    if (web_contents) {
      int frame_id = ExtensionApiFrameIdMap::GetFrameId(render_frame_host);
      if (frame_id != ExtensionApiFrameIdMap::kInvalidFrameId)
        raw_properties->SetInteger("frameId", frame_id);

      // We intentionally don't scrub the tab data here, since the user chose to
      // invoke the extension on the page.
      // TODO(tjudkins) Potentially use GetScrubTabBehavior here to gate based
      // on permissions.
      ExtensionTabUtil::ScrubTabBehavior scrub_tab_behavior = {
          ExtensionTabUtil::kDontScrubTab, ExtensionTabUtil::kDontScrubTab};
      args->Append(ExtensionTabUtil::CreateTabObject(
                       web_contents, scrub_tab_behavior, extension)
                       ->ToValue());
    } else {
      args->Append(std::make_unique<base::DictionaryValue>());
    }
  }

  if (item->type() == MenuItem::CHECKBOX ||
      item->type() == MenuItem::RADIO) {
    bool was_checked = item->checked();
    raw_properties->SetBoolean("wasChecked", was_checked);

    // RADIO items always get set to true when you click on them, but CHECKBOX
    // items get their state toggled.
    bool checked =
        (item->type() == MenuItem::RADIO) ? true : !was_checked;

    item->SetChecked(checked);
    raw_properties->SetBoolean("checked", item->checked());

    if (extension)
      WriteToStorage(extension, item->id().extension_key);
  }

  // Note: web_contents are null in unit tests :(
  if (web_contents && TabHelper::FromWebContents(web_contents)) {
    TabHelper::FromWebContents(web_contents)
        ->active_tab_permission_granter()
        ->GrantIfRequested(extension);
  }

  {
    // Dispatch to menu item's .onclick handler (this is the legacy API, from
    // before chrome.contextMenus.onClicked existed).
    auto event = std::make_unique<Event>(
        webview_guest ? events::WEB_VIEW_INTERNAL_CONTEXT_MENUS
                      : events::CONTEXT_MENUS,
        webview_guest ? kOnWebviewContextMenus : kOnContextMenus,
        std::unique_ptr<base::ListValue>(args->DeepCopy()), context);
    event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
    event_router->DispatchEventToExtension(item->extension_id(),
                                           std::move(event));
  }
  {
    // Dispatch to .contextMenus.onClicked handler.
    auto event = std::make_unique<Event>(
        webview_guest ? events::CHROME_WEB_VIEW_INTERNAL_ON_CLICKED
                      : events::CONTEXT_MENUS_ON_CLICKED,
        webview_guest ? api::chrome_web_view_internal::OnClicked::kEventName
                      : api::context_menus::OnClicked::kEventName,
        std::move(args), context);
    event->user_gesture = EventRouter::USER_GESTURE_ENABLED;
    if (webview_guest)
      event->filter_info.instance_id = webview_guest->view_instance_id();
    event_router->DispatchEventToExtension(item->extension_id(),
                                           std::move(event));
  }
}

void MenuManager::SanitizeRadioListsInMenu(
    const MenuItem::OwnedList& item_list) {
  auto i = item_list.begin();
  while (i != item_list.end()) {
    if ((*i)->type() != MenuItem::RADIO) {
      ++i;
      // Move on to sanitize the next radio list, if any.
      continue;
    }

    // Uncheck any checked radio items in the run, and at the end reset
    // the appropriate one to checked. If no check radio items were found,
    // then check the first radio item in the run.
    auto last_checked = item_list.end();
    MenuItem::OwnedList::const_iterator radio_run_iter;
    for (radio_run_iter = i; radio_run_iter != item_list.end();
        ++radio_run_iter) {
      if ((*radio_run_iter)->type() != MenuItem::RADIO) {
        break;
      }

      if ((*radio_run_iter)->checked()) {
        last_checked = radio_run_iter;
        (*radio_run_iter)->SetChecked(false);
      }
    }

    if (last_checked != item_list.end())
      (*last_checked)->SetChecked(true);
    else
      (*i)->SetChecked(true);

    i = radio_run_iter;
  }
}

bool MenuManager::ItemUpdated(const MenuItem::Id& id) {
  if (!base::Contains(items_by_id_, id))
    return false;

  MenuItem* menu_item = GetItemById(id);
  DCHECK(menu_item);

  if (!menu_item->parent_id()) {
    auto i = context_items_.find(menu_item->id().extension_key);
    if (i == context_items_.end()) {
      NOTREACHED();
      return false;
    }
  }

  // If we selected a radio item, unselect all other items in its group.
  if (menu_item->type() == MenuItem::RADIO && menu_item->checked())
    RadioItemSelected(menu_item);

  return true;
}

void MenuManager::WriteToStorage(const Extension* extension,
                                 const MenuItem::ExtensionKey& extension_key) {
  if (!BackgroundInfo::HasLazyBackgroundPage(extension))
    return;
  // <webview> menu items are transient and not stored in storage.
  if (extension_key.webview_instance_id)
    return;
  const MenuItem::OwnedList* top_items = MenuItems(extension_key);
  MenuItem::List all_items;
  if (top_items) {
    for (auto i = top_items->begin(); i != top_items->end(); ++i) {
      DCHECK(!(*i)->id().extension_key.webview_instance_id);
      (*i)->GetFlattenedSubtree(&all_items);
    }
  }

  if (store_) {
    store_->SetExtensionValue(extension->id(), kContextMenusKey,
                              MenuItemsToValue(all_items));
  }
}

void MenuManager::ReadFromStorage(const std::string& extension_id,
                                  std::unique_ptr<base::Value> value) {
  const Extension* extension = ExtensionRegistry::Get(browser_context_)
                                   ->enabled_extensions()
                                   .GetByID(extension_id);
  if (!extension)
    return;

  MenuItem::OwnedList items = MenuItemsFromValue(extension_id, value.get());
  for (size_t i = 0; i < items.size(); ++i) {
    if (items[i]->parent_id()) {
      // Parent IDs are stored in the parent_id field for convenience, but
      // they have not yet been validated. Separate them out here.
      // Because of the order in which we store items in the prefs, parents will
      // precede children, so we should already know about any parent items.
      std::unique_ptr<MenuItem::Id> parent_id;
      parent_id.swap(items[i]->parent_id_);
      AddChildItem(*parent_id, std::move(items[i]));
    } else {
      AddContextItem(extension, std::move(items[i]));
    }
  }
}

void MenuManager::OnExtensionLoaded(content::BrowserContext* browser_context,
                                    const Extension* extension) {
  if (store_ && BackgroundInfo::HasLazyBackgroundPage(extension)) {
    store_->GetExtensionValue(
        extension->id(),
        kContextMenusKey,
        base::Bind(
            &MenuManager::ReadFromStorage, AsWeakPtr(), extension->id()));
  }

  if (extension->from_bookmark() && UrlHandlers::GetUrlHandlers(extension)) {
    icon_manager_.LoadIcon(browser_context_, extension);
  }
}

void MenuManager::OnExtensionUnloaded(content::BrowserContext* browser_context,
                                      const Extension* extension,
                                      UnloadedExtensionReason reason) {
  MenuItem::ExtensionKey extension_key(extension->id());
  if (base::Contains(context_items_, extension_key)) {
    RemoveAllContextItems(extension_key);
  }
}

void MenuManager::OnOffTheRecordProfileCreated(Profile* off_the_record) {
  observed_profiles_.Add(off_the_record);
}

void MenuManager::OnProfileWillBeDestroyed(Profile* profile) {
  observed_profiles_.Remove(profile);
  if (profile->IsOffTheRecord())
    RemoveAllIncognitoContextItems();
}

gfx::Image MenuManager::GetIconForExtension(const std::string& extension_id) {
  return icon_manager_.GetIcon(extension_id);
}

void MenuManager::RemoveAllIncognitoContextItems() {
  // Get all context menu items with "incognito" set to "split".
  std::set<MenuItem::Id> items_to_remove;
  for (auto iter = items_by_id_.begin(); iter != items_by_id_.end(); ++iter) {
    if (iter->first.incognito)
      items_to_remove.insert(iter->first);
  }

  for (auto remove_iter = items_to_remove.begin();
       remove_iter != items_to_remove.end(); ++remove_iter)
    RemoveContextMenuItem(*remove_iter);
}

MenuItem::ExtensionKey::ExtensionKey()
    : webview_embedder_process_id(ChildProcessHost::kInvalidUniqueID),
      webview_instance_id(kInstanceIDNone) {}

MenuItem::ExtensionKey::ExtensionKey(const std::string& extension_id)
    : extension_id(extension_id),
      webview_embedder_process_id(ChildProcessHost::kInvalidUniqueID),
      webview_instance_id(kInstanceIDNone) {
  DCHECK(!extension_id.empty());
}

MenuItem::ExtensionKey::ExtensionKey(const std::string& extension_id,
                                     int webview_embedder_process_id,
                                     int webview_instance_id)
    : extension_id(extension_id),
      webview_embedder_process_id(webview_embedder_process_id),
      webview_instance_id(webview_instance_id) {
  DCHECK(webview_embedder_process_id != ChildProcessHost::kInvalidUniqueID &&
         webview_instance_id != kInstanceIDNone);
}

bool MenuItem::ExtensionKey::operator==(const ExtensionKey& other) const {
  bool webview_ids_match = webview_instance_id == other.webview_instance_id &&
      webview_embedder_process_id == other.webview_embedder_process_id;

  // If either extension ID is empty, then these ExtensionKeys will be matched
  // only based on the other IDs.
  if (extension_id.empty() || other.extension_id.empty())
    return webview_ids_match;

  return extension_id == other.extension_id && webview_ids_match;
}

bool MenuItem::ExtensionKey::operator<(const ExtensionKey& other) const {
  if (webview_embedder_process_id != other.webview_embedder_process_id)
    return webview_embedder_process_id < other.webview_embedder_process_id;

  if (webview_instance_id != other.webview_instance_id)
    return webview_instance_id < other.webview_instance_id;

  // If either extension ID is empty, then these ExtensionKeys will be compared
  // only based on the other IDs.
  if (extension_id.empty() || other.extension_id.empty())
    return false;

  return extension_id < other.extension_id;
}

bool MenuItem::ExtensionKey::operator!=(const ExtensionKey& other) const {
  return !(*this == other);
}

bool MenuItem::ExtensionKey::empty() const {
  return extension_id.empty() &&
      webview_embedder_process_id == ChildProcessHost::kInvalidUniqueID &&
      webview_instance_id == kInstanceIDNone;
}

MenuItem::Id::Id() : incognito(false), uid(0) {}

MenuItem::Id::Id(bool incognito, const MenuItem::ExtensionKey& extension_key)
    : incognito(incognito), extension_key(extension_key), uid(0) {}

MenuItem::Id::~Id() {
}

bool MenuItem::Id::operator==(const Id& other) const {
  return (incognito == other.incognito &&
          extension_key == other.extension_key && uid == other.uid &&
          string_uid == other.string_uid);
}

bool MenuItem::Id::operator!=(const Id& other) const {
  return !(*this == other);
}

bool MenuItem::Id::operator<(const Id& other) const {
  return std::tie(incognito, extension_key, uid, string_uid) <
    std::tie(other.incognito, other.extension_key, other.uid, other.string_uid);
}

}  // namespace extensions
