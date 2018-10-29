// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmark_manager_private/bookmark_manager_private_api.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/lazy_instance.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/bookmark_stats.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_constants.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/bookmarks/bookmark_drag_drop.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "chrome/common/extensions/api/bookmark_manager_private.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/grit/generated_resources.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node_data.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/browser/scoped_group_bookmark_actions.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/undo/bookmark_undo_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "extensions/browser/extension_registry.h"
#include "extensions/browser/view_type_utils.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::BookmarkNodeData;
using content::WebContents;

namespace extensions {

namespace bookmark_keys = bookmark_api_constants;
namespace bookmark_manager_private = api::bookmark_manager_private;
namespace CanPaste = api::bookmark_manager_private::CanPaste;
namespace Copy = api::bookmark_manager_private::Copy;
namespace CreateWithMetaInfo =
    api::bookmark_manager_private::CreateWithMetaInfo;
namespace Cut = api::bookmark_manager_private::Cut;
namespace Drop = api::bookmark_manager_private::Drop;
namespace GetSubtree = api::bookmark_manager_private::GetSubtree;
namespace GetMetaInfo = api::bookmark_manager_private::GetMetaInfo;
namespace Paste = api::bookmark_manager_private::Paste;
namespace RedoInfo = api::bookmark_manager_private::GetRedoInfo;
namespace RemoveTrees = api::bookmark_manager_private::RemoveTrees;
namespace SetMetaInfo = api::bookmark_manager_private::SetMetaInfo;
namespace SortChildren = api::bookmark_manager_private::SortChildren;
namespace StartDrag = api::bookmark_manager_private::StartDrag;
namespace UndoInfo = api::bookmark_manager_private::GetUndoInfo;
namespace UpdateMetaInfo = api::bookmark_manager_private::UpdateMetaInfo;

namespace {

// Returns a single bookmark node from the argument ID.
// This returns NULL in case of failure.
const BookmarkNode* GetNodeFromString(BookmarkModel* model,
                                      const std::string& id_string) {
  int64_t id;
  if (!base::StringToInt64(id_string, &id))
    return NULL;
  return bookmarks::GetBookmarkNodeByID(model, id);
}

// Gets a vector of bookmark nodes from the argument list of IDs.
// This returns false in the case of failure.
bool GetNodesFromVector(BookmarkModel* model,
                        const std::vector<std::string>& id_strings,
                        std::vector<const BookmarkNode*>* nodes) {
  if (id_strings.empty())
    return false;

  for (size_t i = 0; i < id_strings.size(); ++i) {
    const BookmarkNode* node = GetNodeFromString(model, id_strings[i]);
    if (!node)
      return false;
    nodes->push_back(node);
  }

  return true;
}

// Recursively create a bookmark_manager_private::BookmarkNodeDataElement from
// a bookmark node. This is by used |BookmarkNodeDataToJSON| when the data comes
// from the current profile. In this case we have a BookmarkNode since we got
// the data from the current profile.
bookmark_manager_private::BookmarkNodeDataElement
CreateNodeDataElementFromBookmarkNode(const BookmarkNode& node) {
  bookmark_manager_private::BookmarkNodeDataElement element;
  // Add id and parentId so we can associate the data with existing nodes on the
  // client side.
  element.id.reset(new std::string(base::Int64ToString(node.id())));
  element.parent_id.reset(
      new std::string(base::Int64ToString(node.parent()->id())));

  if (node.is_url())
    element.url.reset(new std::string(node.url().spec()));

  element.title = base::UTF16ToUTF8(node.GetTitle());
  for (int i = 0; i < node.child_count(); ++i) {
    element.children.push_back(
        CreateNodeDataElementFromBookmarkNode(*node.GetChild(i)));
  }

  return element;
}

// Recursively create a bookmark_manager_private::BookmarkNodeDataElement from
// a BookmarkNodeData::Element. This is used by |BookmarkNodeDataToJSON| when
// the data comes from a different profile. When the data comes from a different
// profile we do not have any IDs or parent IDs.
bookmark_manager_private::BookmarkNodeDataElement CreateApiNodeDataElement(
    const BookmarkNodeData::Element& element) {
  bookmark_manager_private::BookmarkNodeDataElement node_element;

  if (element.is_url)
    node_element.url.reset(new std::string(element.url.spec()));
  node_element.title = base::UTF16ToUTF8(element.title);
  for (size_t i = 0; i < element.children.size(); ++i) {
    node_element.children.push_back(
        CreateApiNodeDataElement(element.children[i]));
  }

  return node_element;
}

// Creates a bookmark_manager_private::BookmarkNodeData from a BookmarkNodeData.
bookmark_manager_private::BookmarkNodeData CreateApiBookmarkNodeData(
    Profile* profile,
    const BookmarkNodeData& data) {
  const base::FilePath& profile_path = profile->GetPath();

  bookmark_manager_private::BookmarkNodeData node_data;
  node_data.same_profile = data.IsFromProfilePath(profile_path);

  if (node_data.same_profile) {
    std::vector<const BookmarkNode*> nodes = data.GetNodes(
        BookmarkModelFactory::GetForBrowserContext(profile), profile_path);
    for (size_t i = 0; i < nodes.size(); ++i) {
      node_data.elements.push_back(
          CreateNodeDataElementFromBookmarkNode(*nodes[i]));
    }
  } else {
    // We do not have a node IDs when the data comes from a different profile.
    for (size_t i = 0; i < data.size(); ++i)
      node_data.elements.push_back(CreateApiNodeDataElement(data.elements[i]));
  }
  return node_data;
}

}  // namespace

BookmarkManagerPrivateEventRouter::BookmarkManagerPrivateEventRouter(
    content::BrowserContext* browser_context,
    BookmarkModel* bookmark_model)
    : browser_context_(browser_context), bookmark_model_(bookmark_model) {
  bookmark_model_->AddObserver(this);
}

BookmarkManagerPrivateEventRouter::~BookmarkManagerPrivateEventRouter() {
  if (bookmark_model_)
    bookmark_model_->RemoveObserver(this);
}

void BookmarkManagerPrivateEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> event_args) {
  EventRouter::Get(browser_context_)
      ->BroadcastEvent(std::make_unique<Event>(histogram_value, event_name,
                                               std::move(event_args)));
}

void BookmarkManagerPrivateEventRouter::BookmarkModelChanged() {}

void BookmarkManagerPrivateEventRouter::BookmarkModelBeingDeleted(
    BookmarkModel* model) {
  bookmark_model_ = NULL;
}

void BookmarkManagerPrivateEventRouter::OnWillChangeBookmarkMetaInfo(
    BookmarkModel* model,
    const BookmarkNode* node) {
  DCHECK(prev_meta_info_.empty());
  if (node->GetMetaInfoMap())
    prev_meta_info_ = *node->GetMetaInfoMap();
}

void BookmarkManagerPrivateEventRouter::BookmarkMetaInfoChanged(
    BookmarkModel* model,
    const BookmarkNode* node) {
  const BookmarkNode::MetaInfoMap* new_meta_info = node->GetMetaInfoMap();
  bookmark_manager_private::MetaInfoFields changes;

  // Identify changed/removed fields:
  for (BookmarkNode::MetaInfoMap::const_iterator it = prev_meta_info_.begin();
       it != prev_meta_info_.end();
       ++it) {
    if (!new_meta_info) {
      changes.additional_properties[it->first] = "";
    } else {
      auto new_meta_field = new_meta_info->find(it->first);
      if (new_meta_field == new_meta_info->end()) {
        changes.additional_properties[it->first] = "";
      } else if (it->second != new_meta_field->second) {
        changes.additional_properties[it->first] = new_meta_field->second;
      }
    }
  }

  // Identify added fields:
  if (new_meta_info) {
    for (auto it = new_meta_info->cbegin(); it != new_meta_info->cend(); ++it) {
      auto prev_meta_field = prev_meta_info_.find(it->first);
      if (prev_meta_field == prev_meta_info_.end())
        changes.additional_properties[it->first] = it->second;
    }
  }

  prev_meta_info_.clear();
  DispatchEvent(events::BOOKMARK_MANAGER_PRIVATE_ON_META_INFO_CHANGED,
                bookmark_manager_private::OnMetaInfoChanged::kEventName,
                bookmark_manager_private::OnMetaInfoChanged::Create(
                    base::Int64ToString(node->id()), changes));
}

BookmarkManagerPrivateAPI::BookmarkManagerPrivateAPI(
    content::BrowserContext* browser_context)
    : browser_context_(browser_context) {
  EventRouter* event_router = EventRouter::Get(browser_context);
  event_router->RegisterObserver(
      this, bookmark_manager_private::OnMetaInfoChanged::kEventName);
}

BookmarkManagerPrivateAPI::~BookmarkManagerPrivateAPI() {}

void BookmarkManagerPrivateAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<
    BrowserContextKeyedAPIFactory<BookmarkManagerPrivateAPI>>::DestructorAtExit
    g_bookmark_manager_private_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BookmarkManagerPrivateAPI>*
BookmarkManagerPrivateAPI::GetFactoryInstance() {
  return g_bookmark_manager_private_api_factory.Pointer();
}

void BookmarkManagerPrivateAPI::OnListenerAdded(
    const EventListenerInfo& details) {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
  event_router_.reset(new BookmarkManagerPrivateEventRouter(
      browser_context_,
      BookmarkModelFactory::GetForBrowserContext(browser_context_)));
}

BookmarkManagerPrivateDragEventRouter::BookmarkManagerPrivateDragEventRouter(
    content::WebContents* web_contents)
    : web_contents_(web_contents),
      profile_(
          Profile::FromBrowserContext(web_contents_->GetBrowserContext())) {
  // We need to guarantee the BookmarkTabHelper is created.
  BookmarkTabHelper::CreateForWebContents(web_contents_);
  BookmarkTabHelper* bookmark_tab_helper =
      BookmarkTabHelper::FromWebContents(web_contents_);
  bookmark_tab_helper->set_bookmark_drag_delegate(this);
}

BookmarkManagerPrivateDragEventRouter::
    ~BookmarkManagerPrivateDragEventRouter() {
  // No need to remove ourselves as the BookmarkTabHelper's delegate, since they
  // are both WebContentsUserData and will be deleted at the same time.
}

void BookmarkManagerPrivateDragEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    std::unique_ptr<base::ListValue> args) {
  EventRouter* event_router = EventRouter::Get(profile_);
  if (!event_router)
    return;

  std::unique_ptr<Event> event(
      new Event(histogram_value, event_name, std::move(args)));
  event_router->BroadcastEvent(std::move(event));
}

void BookmarkManagerPrivateDragEventRouter::OnDragEnter(
    const BookmarkNodeData& data) {
  if (!data.is_valid())
    return;
  DispatchEvent(events::BOOKMARK_MANAGER_PRIVATE_ON_DRAG_ENTER,
                bookmark_manager_private::OnDragEnter::kEventName,
                bookmark_manager_private::OnDragEnter::Create(
                    CreateApiBookmarkNodeData(profile_, data)));
}

void BookmarkManagerPrivateDragEventRouter::OnDragOver(
    const BookmarkNodeData& data) {
  // Intentionally empty since these events happens too often and floods the
  // message queue. We do not need this event for the bookmark manager anyway.
}

void BookmarkManagerPrivateDragEventRouter::OnDragLeave(
    const BookmarkNodeData& data) {
  if (!data.is_valid())
    return;
  DispatchEvent(events::BOOKMARK_MANAGER_PRIVATE_ON_DRAG_LEAVE,
                bookmark_manager_private::OnDragLeave::kEventName,
                bookmark_manager_private::OnDragLeave::Create(
                    CreateApiBookmarkNodeData(profile_, data)));
}

void BookmarkManagerPrivateDragEventRouter::OnDrop(
    const BookmarkNodeData& data) {
  if (!data.is_valid())
    return;
  DispatchEvent(events::BOOKMARK_MANAGER_PRIVATE_ON_DROP,
                bookmark_manager_private::OnDrop::kEventName,
                bookmark_manager_private::OnDrop::Create(
                    CreateApiBookmarkNodeData(profile_, data)));

  // Make a copy that is owned by this instance.
  ClearBookmarkNodeData();
  bookmark_drag_data_ = data;
}

const BookmarkNodeData*
BookmarkManagerPrivateDragEventRouter::GetBookmarkNodeData() {
  if (bookmark_drag_data_.is_valid())
    return &bookmark_drag_data_;
  return NULL;
}

void BookmarkManagerPrivateDragEventRouter::ClearBookmarkNodeData() {
  bookmark_drag_data_.Clear();
}

bool ClipboardBookmarkManagerFunction::CopyOrCut(bool cut,
    const std::vector<std::string>& id_list) {
  BookmarkModel* model = GetBookmarkModel();
  std::vector<const BookmarkNode*> nodes;
  if (!GetNodesFromVector(model, id_list, &nodes)) {
    error_ = "Could not find bookmark nodes with given ids.";
    return false;
  }
  bookmarks::ManagedBookmarkService* managed = GetManagedBookmarkService();
  if (cut && bookmarks::HasDescendantsOf(nodes, managed->managed_node())) {
    error_ = bookmark_keys::kModifyManagedError;
    return false;
  }
  bookmarks::CopyToClipboard(model, nodes, cut);
  return true;
}

bool BookmarkManagerPrivateCopyFunction::RunOnReady() {
  std::unique_ptr<Copy::Params> params(Copy::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  return CopyOrCut(false, params->id_list);
}

bool BookmarkManagerPrivateCutFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<Cut::Params> params(Cut::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  return CopyOrCut(true, params->id_list);
}

bool BookmarkManagerPrivatePasteFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<Paste::Params> params(Paste::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  const BookmarkNode* parent_node = GetNodeFromString(model, params->parent_id);
  if (!CanBeModified(parent_node))
    return false;
  bool can_paste = bookmarks::CanPasteFromClipboard(model, parent_node);
  if (!can_paste)
    return false;

  // We want to use the highest index of the selected nodes as a destination.
  std::vector<const BookmarkNode*> nodes;
  // No need to test return value, if we got an empty list, we insert at end.
  if (params->selected_id_list)
    GetNodesFromVector(model, *params->selected_id_list, &nodes);
  int highest_index = -1;  // -1 means insert at end of list.
  for (size_t i = 0; i < nodes.size(); ++i) {
    // + 1 so that we insert after the selection.
    int index = parent_node->GetIndexOf(nodes[i]) + 1;
    if (index > highest_index)
      highest_index = index;
  }

  bookmarks::PasteFromClipboard(model, parent_node, highest_index);
  return true;
}

bool BookmarkManagerPrivateCanPasteFunction::RunOnReady() {
  std::unique_ptr<CanPaste::Params> params(CanPaste::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  PrefService* prefs = user_prefs::UserPrefs::Get(GetProfile());
  if (!prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled)) {
    SetResult(std::make_unique<base::Value>(false));
    return true;
  }

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  const BookmarkNode* parent_node = GetNodeFromString(model, params->parent_id);
  if (!parent_node) {
    error_ = bookmark_keys::kNoParentError;
    return false;
  }
  bool can_paste = bookmarks::CanPasteFromClipboard(model, parent_node);
  SetResult(std::make_unique<base::Value>(can_paste));
  return true;
}

bool BookmarkManagerPrivateSortChildrenFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<SortChildren::Params> params(
      SortChildren::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  const BookmarkNode* parent_node = GetNodeFromString(model, params->parent_id);
  if (!CanBeModified(parent_node))
    return false;
  model->SortChildren(parent_node);
  return true;
}

ExtensionFunction::ResponseAction
BookmarkManagerPrivateGetStringsFunction::Run() {
  std::unique_ptr<base::DictionaryValue> localized_strings(
      new base::DictionaryValue());

  localized_strings->SetString("title",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_TITLE));
  localized_strings->SetString("search_button",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_SEARCH_BUTTON));
  localized_strings->SetString("folders_menu",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_FOLDERS_MENU));
  localized_strings->SetString("organize_menu",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_ORGANIZE_MENU));
  localized_strings->SetString("show_in_folder",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_SHOW_IN_FOLDER));
  localized_strings->SetString("sort",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_SORT));
  localized_strings->SetString("import_menu",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_IMPORT_MENU));
  localized_strings->SetString("export_menu",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_EXPORT_MENU));
  localized_strings->SetString("rename_folder",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_RENAME_FOLDER));
  localized_strings->SetString("edit",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_EDIT));
  localized_strings->SetString("should_open_all",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_SHOULD_OPEN_ALL));
  localized_strings->SetString("open_incognito",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_INCOGNITO));
  localized_strings->SetString("open_in_new_tab",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_IN_NEW_TAB));
  localized_strings->SetString("open_in_new_window",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_IN_NEW_WINDOW));
  localized_strings->SetString("add_new_bookmark",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_ADD_NEW_BOOKMARK));
  localized_strings->SetString("new_folder",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_NEW_FOLDER));
  localized_strings->SetString("open_all",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_ALL));
  localized_strings->SetString("open_all_new_window",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_ALL_NEW_WINDOW));
  localized_strings->SetString("open_all_incognito",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_OPEN_ALL_INCOGNITO));
  localized_strings->SetString("remove",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_BAR_REMOVE));
  localized_strings->SetString("copy",
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_COPY));
  localized_strings->SetString("cut",
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_CUT));
  localized_strings->SetString("paste",
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_PASTE));
  localized_strings->SetString("delete",
      l10n_util::GetStringUTF16(IDS_CONTENT_CONTEXT_DELETE));
  localized_strings->SetString("undo_delete",
      l10n_util::GetStringUTF16(IDS_UNDO_DELETE));
  localized_strings->SetString("new_folder_name",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_EDITOR_NEW_FOLDER_NAME));
  localized_strings->SetString("name_input_placeholder",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_NAME_INPUT_PLACE_HOLDER));
  localized_strings->SetString("url_input_placeholder",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_URL_INPUT_PLACE_HOLDER));
  localized_strings->SetString("invalid_url",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_INVALID_URL));
  localized_strings->SetString("recent",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_RECENT));
  localized_strings->SetString("search",
      l10n_util::GetStringUTF16(IDS_BOOKMARK_MANAGER_SEARCH));
  localized_strings->SetString("save",
      l10n_util::GetStringUTF16(IDS_SAVE));
  localized_strings->SetString("cancel",
      l10n_util::GetStringUTF16(IDS_CANCEL));

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  webui::SetLoadTimeDataDefaults(app_locale, localized_strings.get());

  return RespondNow(OneArgument(std::move(localized_strings)));
}

bool BookmarkManagerPrivateStartDragFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  content::WebContents* web_contents = GetSenderWebContents();
  if (GetViewType(web_contents) != VIEW_TYPE_TAB_CONTENTS) {
    NOTREACHED();
    return false;
  }

  std::unique_ptr<StartDrag::Params> params(StartDrag::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  std::vector<const BookmarkNode*> nodes;
  if (!GetNodesFromVector(model, params->id_list, &nodes))
    return false;

  ui::DragDropTypes::DragEventSource source =
      ui::DragDropTypes::DRAG_EVENT_SOURCE_MOUSE;
  if (params->is_from_touch)
    source = ui::DragDropTypes::DRAG_EVENT_SOURCE_TOUCH;

  chrome::DragBookmarks(GetProfile(),
                        {
                            std::move(nodes), params->drag_node_index,
                            platform_util::GetViewForWindow(
                                web_contents->GetTopLevelNativeWindow()),
                            source,
                        });

  return true;
}

bool BookmarkManagerPrivateDropFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<Drop::Params> params(Drop::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());

  const BookmarkNode* drop_parent = GetNodeFromString(model, params->parent_id);
  if (!CanBeModified(drop_parent))
    return false;

  content::WebContents* web_contents = GetSenderWebContents();
  if (GetViewType(web_contents) != VIEW_TYPE_TAB_CONTENTS) {
    NOTREACHED();
    return false;
  }

  int drop_index;
  if (params->index)
    drop_index = *params->index;
  else
    drop_index = drop_parent->child_count();

  BookmarkManagerPrivateDragEventRouter* router =
      BookmarkManagerPrivateDragEventRouter::FromWebContents(web_contents);

  DCHECK(router);
  const BookmarkNodeData* drag_data = router->GetBookmarkNodeData();
  if (drag_data == NULL) {
    NOTREACHED() <<"Somehow we're dropping null bookmark data";
    return false;
  }
  const bool copy = false;
  chrome::DropBookmarks(
      GetProfile(), *drag_data, drop_parent, drop_index, copy);

  router->ClearBookmarkNodeData();
  return true;
}

bool BookmarkManagerPrivateGetSubtreeFunction::RunOnReady() {
  std::unique_ptr<GetSubtree::Params> params(
      GetSubtree::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const BookmarkNode* node = NULL;

  if (params->id.empty()) {
    BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(GetProfile());
    node = model->root_node();
  } else {
    node = GetBookmarkNodeFromId(params->id);
    if (!node)
      return false;
  }

  std::vector<api::bookmarks::BookmarkTreeNode> nodes;
  bookmarks::ManagedBookmarkService* managed = GetManagedBookmarkService();
  if (params->folders_only)
    bookmark_api_helpers::AddNodeFoldersOnly(managed, node, &nodes, true);
  else
    bookmark_api_helpers::AddNode(managed, node, &nodes, true);
  results_ = GetSubtree::Results::Create(nodes);
  return true;
}

bool BookmarkManagerPrivateCanEditFunction::RunOnReady() {
  PrefService* prefs = user_prefs::UserPrefs::Get(GetProfile());
  SetResult(std::make_unique<base::Value>(
      prefs->GetBoolean(bookmarks::prefs::kEditBookmarksEnabled)));
  return true;
}

bool BookmarkManagerPrivateRecordLaunchFunction::RunOnReady() {
  RecordBookmarkLaunch(NULL, BOOKMARK_LAUNCH_LOCATION_MANAGER);
  return true;
}

bool BookmarkManagerPrivateCreateWithMetaInfoFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<CreateWithMetaInfo::Params> params(
      CreateWithMetaInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  const BookmarkNode* node = CreateBookmarkNode(
      model, params->bookmark, &params->meta_info.additional_properties);
  if (!node)
    return false;

  api::bookmarks::BookmarkTreeNode result_node =
      bookmark_api_helpers::GetBookmarkTreeNode(GetManagedBookmarkService(),
                                                node, false, false);
  results_ = CreateWithMetaInfo::Results::Create(result_node);

  return true;
}

bool BookmarkManagerPrivateGetMetaInfoFunction::RunOnReady() {
  std::unique_ptr<GetMetaInfo::Params> params(
      GetMetaInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  if (params->id) {
    const BookmarkNode* node = GetBookmarkNodeFromId(*params->id);
    if (!node)
      return false;

    if (params->key) {
      std::string value;
      if (node->GetMetaInfo(*params->key, &value)) {
        GetMetaInfo::Results::Value result;
        result.as_string.reset(new std::string(value));
        results_ = GetMetaInfo::Results::Create(result);
      }
    } else {
      GetMetaInfo::Results::Value result;
      result.as_object.reset(new GetMetaInfo::Results::Value::Object);

      const BookmarkNode::MetaInfoMap* meta_info = node->GetMetaInfoMap();
      if (meta_info) {
        BookmarkNode::MetaInfoMap::const_iterator itr;
        base::DictionaryValue& temp = result.as_object->additional_properties;
        for (itr = meta_info->begin(); itr != meta_info->end(); itr++) {
          temp.SetKey(itr->first, base::Value(itr->second));
        }
      }
      results_ = GetMetaInfo::Results::Create(result);
    }
  } else {
    if (params->key) {
      error_ = bookmark_api_constants::kInvalidParamError;
      return true;
    }

    BookmarkModel* model =
        BookmarkModelFactory::GetForBrowserContext(GetProfile());
    const BookmarkNode* node = model->root_node();

    GetMetaInfo::Results::Value result;
    result.as_object.reset(new GetMetaInfo::Results::Value::Object);

    bookmark_api_helpers::GetMetaInfo(*node,
        &result.as_object->additional_properties);

    results_ = GetMetaInfo::Results::Create(result);
  }

  return true;
}

bool BookmarkManagerPrivateSetMetaInfoFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<SetMetaInfo::Params> params(
      SetMetaInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const BookmarkNode* node = GetBookmarkNodeFromId(params->id);
  if (!node)
    return false;

  if (!CanBeModified(node))
    return false;

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  if (model->is_permanent_node(node)) {
    error_ = bookmark_keys::kModifySpecialError;
    return false;
  }

  model->SetNodeMetaInfo(node, params->key, params->value);
  return true;
}

bool BookmarkManagerPrivateUpdateMetaInfoFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<UpdateMetaInfo::Params> params(
      UpdateMetaInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  const BookmarkNode* node = GetBookmarkNodeFromId(params->id);
  if (!node)
    return false;

  if (!CanBeModified(node))
    return false;

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  if (model->is_permanent_node(node)) {
    error_ = bookmark_keys::kModifySpecialError;
    return false;
  }

  BookmarkNode::MetaInfoMap new_meta_info(
      params->meta_info_changes.additional_properties);
  if (node->GetMetaInfoMap()) {
    new_meta_info.insert(node->GetMetaInfoMap()->begin(),
                         node->GetMetaInfoMap()->end());
  }
  model->SetNodeMetaInfoMap(node, new_meta_info);

  return true;
}

bool BookmarkManagerPrivateRemoveTreesFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  std::unique_ptr<RemoveTrees::Params> params(
      RemoveTrees::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params);

  BookmarkModel* model = GetBookmarkModel();
  bookmarks::ManagedBookmarkService* managed = GetManagedBookmarkService();
  bookmarks::ScopedGroupBookmarkActions group_deletes(model);
  int64_t id;
  for (size_t i = 0; i < params->id_list.size(); ++i) {
    if (!GetBookmarkIdAsInt64(params->id_list[i], &id))
      return false;
    if (!bookmark_api_helpers::RemoveNode(model, managed, id, true, &error_))
      return false;
  }

  return true;
}

bool BookmarkManagerPrivateUndoFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  BookmarkUndoServiceFactory::GetForProfile(GetProfile())->undo_manager()->
      Undo();
  return true;
}

bool BookmarkManagerPrivateRedoFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return false;

  BookmarkUndoServiceFactory::GetForProfile(GetProfile())->undo_manager()->
      Redo();
  return true;
}

bool BookmarkManagerPrivateGetUndoInfoFunction::RunOnReady() {
  UndoManager* undo_manager =
      BookmarkUndoServiceFactory::GetForProfile(GetProfile())->undo_manager();

  UndoInfo::Results::Result result;
  result.enabled = undo_manager->undo_count() > 0;
  result.label = base::UTF16ToUTF8(undo_manager->GetUndoLabel());

  results_ = UndoInfo::Results::Create(result);
  return true;
}

bool BookmarkManagerPrivateGetRedoInfoFunction::RunOnReady() {
  UndoManager* undo_manager =
      BookmarkUndoServiceFactory::GetForProfile(GetProfile())->undo_manager();

  RedoInfo::Results::Result result;
  result.enabled = undo_manager->redo_count() > 0;
  result.label = base::UTF16ToUTF8(undo_manager->GetRedoLabel());

  results_ = RedoInfo::Results::Create(result);
  return true;
}

}  // namespace extensions
