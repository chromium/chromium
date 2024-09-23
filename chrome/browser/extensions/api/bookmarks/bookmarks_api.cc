// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/bookmarks/bookmarks_api.h"

#include <stddef.h>

#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/lazy_instance.h"
#include "base/path_service.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/extensions/api/bookmarks/bookmark_api_helpers.h"
#include "chrome/browser/extensions/bookmarks/bookmarks_error_constants.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/chrome_select_file_policy.h"
#include "chrome/common/extensions/api/bookmarks.h"
#include "chrome/common/importer/importer_data_types.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/bookmarks/common/bookmark_metrics.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_dispatcher.h"

using bookmarks::BookmarkModel;
using bookmarks::BookmarkNode;
using bookmarks::ManagedBookmarkService;

namespace extensions {

using api::bookmarks::BookmarkTreeNode;
using api::bookmarks::CreateDetails;
using content::BrowserContext;
using content::BrowserThread;
using content::WebContents;

BookmarkEventRouter::BookmarkEventRouter(Profile* profile)
    : browser_context_(profile),
      model_(BookmarkModelFactory::GetForBrowserContext(profile)),
      managed_(ManagedBookmarkServiceFactory::GetForProfile(profile)) {
  model_->AddObserver(this);
}

BookmarkEventRouter::~BookmarkEventRouter() {
  if (model_) {
    model_->RemoveObserver(this);
  }
}

void BookmarkEventRouter::DispatchEvent(events::HistogramValue histogram_value,
                                        const std::string& event_name,
                                        base::Value::List event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    event_router->BroadcastEvent(std::make_unique<extensions::Event>(
        histogram_value, event_name, std::move(event_args)));
  }
}

void BookmarkEventRouter::BookmarkModelLoaded(bool ids_reassigned) {
  // TODO(erikkay): Perhaps we should send this event down to the extension
  // so they know when it's safe to use the API?
}

void BookmarkEventRouter::BookmarkModelBeingDeleted() {
  model_ = nullptr;
}

void BookmarkEventRouter::BookmarkNodeMoved(const BookmarkNode* old_parent,
                                            size_t old_index,
                                            const BookmarkNode* new_parent,
                                            size_t new_index) {
  const BookmarkNode* node = new_parent->children()[new_index].get();
  api::bookmarks::OnMoved::MoveInfo move_info;
  move_info.parent_id = base::NumberToString(new_parent->id());
  move_info.index = static_cast<int>(new_index);
  move_info.old_parent_id = base::NumberToString(old_parent->id());
  move_info.old_index = static_cast<int>(old_index);

  DispatchEvent(events::BOOKMARKS_ON_MOVED, api::bookmarks::OnMoved::kEventName,
                api::bookmarks::OnMoved::Create(
                    base::NumberToString(node->id()), move_info));
}

void BookmarkEventRouter::BookmarkNodeAdded(const BookmarkNode* parent,
                                            size_t index,
                                            bool added_by_user) {
  const BookmarkNode* node = parent->children()[index].get();
  BookmarkTreeNode tree_node =
      bookmark_api_helpers::GetBookmarkTreeNode(managed_, node, false, false);
  DispatchEvent(events::BOOKMARKS_ON_CREATED,
                api::bookmarks::OnCreated::kEventName,
                api::bookmarks::OnCreated::Create(
                    base::NumberToString(node->id()), tree_node));
}

void BookmarkEventRouter::BookmarkNodeRemoved(
    const BookmarkNode* parent,
    size_t index,
    const BookmarkNode* node,
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  api::bookmarks::OnRemoved::RemoveInfo remove_info;
  remove_info.parent_id = base::NumberToString(parent->id());
  remove_info.index = static_cast<int>(index);
  bookmark_api_helpers::PopulateBookmarkTreeNode(managed_, node, true, false,
                                                 &remove_info.node);

  DispatchEvent(events::BOOKMARKS_ON_REMOVED,
                api::bookmarks::OnRemoved::kEventName,
                api::bookmarks::OnRemoved::Create(
                    base::NumberToString(node->id()), remove_info));
}

void BookmarkEventRouter::BookmarkAllUserNodesRemoved(
    const std::set<GURL>& removed_urls,
    const base::Location& location) {
  // TODO(crbug.com/40277078): This used to be used only on Android, but that's
  // no longer the case. We need to implement a new event to handle this.
}

void BookmarkEventRouter::BookmarkNodeChanged(const BookmarkNode* node) {
  // TODO(erikkay) The only three things that BookmarkModel sends this
  // notification for are title, url and favicon.  Since we're currently
  // ignoring favicon and since the notification doesn't say which one anyway,
  // for now we only include title and url.  The ideal thing would be to change
  // BookmarkModel to indicate what changed.
  api::bookmarks::OnChanged::ChangeInfo change_info;
  change_info.title = base::UTF16ToUTF8(node->GetTitle());
  if (node->is_url())
    change_info.url = node->url().spec();

  DispatchEvent(events::BOOKMARKS_ON_CHANGED,
                api::bookmarks::OnChanged::kEventName,
                api::bookmarks::OnChanged::Create(
                    base::NumberToString(node->id()), change_info));
}

void BookmarkEventRouter::BookmarkNodeFaviconChanged(const BookmarkNode* node) {
  // TODO(erikkay) anything we should do here?
}

void BookmarkEventRouter::BookmarkNodeChildrenReordered(
    const BookmarkNode* node) {
  api::bookmarks::OnChildrenReordered::ReorderInfo reorder_info;
  for (const auto& child : node->children())
    reorder_info.child_ids.push_back(base::NumberToString(child->id()));

  DispatchEvent(events::BOOKMARKS_ON_CHILDREN_REORDERED,
                api::bookmarks::OnChildrenReordered::kEventName,
                api::bookmarks::OnChildrenReordered::Create(
                    base::NumberToString(node->id()), reorder_info));
}

void BookmarkEventRouter::ExtensiveBookmarkChangesBeginning() {
  DispatchEvent(events::BOOKMARKS_ON_IMPORT_BEGAN,
                api::bookmarks::OnImportBegan::kEventName,
                api::bookmarks::OnImportBegan::Create());
}

void BookmarkEventRouter::ExtensiveBookmarkChangesEnded() {
  DispatchEvent(events::BOOKMARKS_ON_IMPORT_ENDED,
                api::bookmarks::OnImportEnded::kEventName,
                api::bookmarks::OnImportEnded::Create());
}

BookmarksAPI::BookmarksAPI(BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  event_router->RegisterObserver(this, api::bookmarks::OnCreated::kEventName);
  event_router->RegisterObserver(this, api::bookmarks::OnRemoved::kEventName);
  event_router->RegisterObserver(this, api::bookmarks::OnChanged::kEventName);
  event_router->RegisterObserver(this, api::bookmarks::OnMoved::kEventName);
  event_router->RegisterObserver(
      this, api::bookmarks::OnChildrenReordered::kEventName);
  event_router->RegisterObserver(this,
                                 api::bookmarks::OnImportBegan::kEventName);
  event_router->RegisterObserver(this,
                                 api::bookmarks::OnImportEnded::kEventName);
}

BookmarksAPI::~BookmarksAPI() {
}

void BookmarksAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<BookmarksAPI>>::
    DestructorAtExit g_bookmarks_api_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<BookmarksAPI>*
BookmarksAPI::GetFactoryInstance() {
  return g_bookmarks_api_factory.Pointer();
}

void BookmarksAPI::OnListenerAdded(const EventListenerInfo& details) {
  bookmark_event_router_ = std::make_unique<BookmarkEventRouter>(
      Profile::FromBrowserContext(browser_context_));
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

ExtensionFunction::ResponseValue BookmarksGetFunction::RunOnReady() {
  std::optional<api::bookmarks::Get::Params> params =
      api::bookmarks::Get::Params::Create(args());
  if (!params)
    return BadMessage();

  std::vector<BookmarkTreeNode> nodes;
  ManagedBookmarkService* managed = GetManagedBookmarkService();
  if (params->id_or_id_list.as_strings) {
    std::vector<std::string>& ids = *params->id_or_id_list.as_strings;
    size_t count = ids.size();
    if (count <= 0)
      return BadMessage();
    for (size_t i = 0; i < count; ++i) {
      std::string error;
      const BookmarkNode* node = GetBookmarkNodeFromId(ids[i], &error);
      if (!node)
        return Error(error);
      bookmark_api_helpers::AddNode(managed, node, &nodes, false);
    }
  } else {
    std::string error;
    const BookmarkNode* node =
        GetBookmarkNodeFromId(*params->id_or_id_list.as_string, &error);
    if (!node)
      return Error(error);
    bookmark_api_helpers::AddNode(managed, node, &nodes, false);
  }

  return ArgumentList(api::bookmarks::Get::Results::Create(nodes));
}

ExtensionFunction::ResponseValue BookmarksGetChildrenFunction::RunOnReady() {
  std::optional<api::bookmarks::GetChildren::Params> params =
      api::bookmarks::GetChildren::Params::Create(args());
  if (!params)
    return BadMessage();

  std::string error;
  const BookmarkNode* node = GetBookmarkNodeFromId(params->id, &error);
  if (!node)
    return Error(error);

  std::vector<BookmarkTreeNode> nodes;
  for (const auto& child : node->children()) {
    bookmark_api_helpers::AddNode(GetManagedBookmarkService(), child.get(),
                                  &nodes, false);
  }

  return ArgumentList(api::bookmarks::GetChildren::Results::Create(nodes));
}

ExtensionFunction::ResponseValue BookmarksGetRecentFunction::RunOnReady() {
  std::optional<api::bookmarks::GetRecent::Params> params =
      api::bookmarks::GetRecent::Params::Create(args());
  if (!params)
    return BadMessage();
  if (params->number_of_items < 1) {
    // TODO(lazyboy): This shouldn't be necessary as schema specifies
    // "minimum: 1".
    return Error("numberOfItems cannot be less than 1.");
  }

  std::vector<const BookmarkNode*> nodes;
  bookmarks::GetMostRecentlyAddedEntries(
      BookmarkModelFactory::GetForBrowserContext(GetProfile()),
      params->number_of_items, &nodes);

  std::vector<BookmarkTreeNode> tree_nodes;
  for (const BookmarkNode* node : nodes) {
    bookmark_api_helpers::AddNode(GetManagedBookmarkService(), node,
                                  &tree_nodes, false);
  }

  return ArgumentList(api::bookmarks::GetRecent::Results::Create(tree_nodes));
}

ExtensionFunction::ResponseValue BookmarksGetTreeFunction::RunOnReady() {
  std::vector<BookmarkTreeNode> nodes;
  const BookmarkNode* node =
      BookmarkModelFactory::GetForBrowserContext(GetProfile())->root_node();
  bookmark_api_helpers::AddNode(GetManagedBookmarkService(), node, &nodes,
                                true);
  return ArgumentList(api::bookmarks::GetTree::Results::Create(nodes));
}

ExtensionFunction::ResponseValue BookmarksGetSubTreeFunction::RunOnReady() {
  std::optional<api::bookmarks::GetSubTree::Params> params =
      api::bookmarks::GetSubTree::Params::Create(args());
  if (!params)
    return BadMessage();

  std::string error;
  const BookmarkNode* node = GetBookmarkNodeFromId(params->id, &error);
  if (!node)
    return Error(error);

  std::vector<BookmarkTreeNode> nodes;
  bookmark_api_helpers::AddNode(GetManagedBookmarkService(), node, &nodes,
                                true);
  return ArgumentList(api::bookmarks::GetSubTree::Results::Create(nodes));
}

ExtensionFunction::ResponseValue BookmarksSearchFunction::RunOnReady() {
  std::optional<api::bookmarks::Search::Params> params =
      api::bookmarks::Search::Params::Create(args());
  if (!params)
    return BadMessage();

  std::vector<const BookmarkNode*> nodes;
  if (params->query.as_string) {
    bookmarks::QueryFields query;
    query.word_phrase_query = std::make_unique<std::u16string>(
        base::UTF8ToUTF16(*params->query.as_string));
    nodes = bookmarks::GetBookmarksMatchingProperties(
        BookmarkModelFactory::GetForBrowserContext(GetProfile()), query,
        std::numeric_limits<int>::max());
  } else {
    DCHECK(params->query.as_object);
    const api::bookmarks::Search::Params::Query::Object& object =
        *params->query.as_object;
    bookmarks::QueryFields query;
    if (object.query) {
      query.word_phrase_query =
          std::make_unique<std::u16string>(base::UTF8ToUTF16(*object.query));
    }
    if (object.url)
      query.url =
          std::make_unique<std::u16string>(base::UTF8ToUTF16(*object.url));
    if (object.title)
      query.title =
          std::make_unique<std::u16string>(base::UTF8ToUTF16(*object.title));
    nodes = bookmarks::GetBookmarksMatchingProperties(
        BookmarkModelFactory::GetForBrowserContext(GetProfile()), query,
        std::numeric_limits<int>::max());
  }

  std::vector<BookmarkTreeNode> tree_nodes;
  ManagedBookmarkService* managed = GetManagedBookmarkService();
  for (const BookmarkNode* node : nodes)
    bookmark_api_helpers::AddNode(managed, node, &tree_nodes, false);

  return ArgumentList(api::bookmarks::Search::Results::Create(tree_nodes));
}

ExtensionFunction::ResponseValue BookmarksRemoveFunctionBase::RunOnReady() {
  if (!EditBookmarksEnabled())
    return Error(bookmarks_errors::kEditBookmarksDisabled);

  std::optional<api::bookmarks::Remove::Params> params =
      api::bookmarks::Remove::Params::Create(args());
  if (!params)
    return BadMessage();

  int64_t id;
  if (!base::StringToInt64(params->id, &id))
    return Error(bookmarks_errors::kInvalidIdError);

  std::string error;
  BookmarkModel* model = GetBookmarkModel();
  ManagedBookmarkService* managed = GetManagedBookmarkService();
  if (!bookmark_api_helpers::RemoveNode(model, managed, id, is_recursive(),
                                        &error)) {
    return Error(error);
  }

  return NoArguments();
}

bool BookmarksRemoveFunction::is_recursive() const {
  return false;
}

bool BookmarksRemoveTreeFunction::is_recursive() const {
  return true;
}

ExtensionFunction::ResponseValue BookmarksCreateFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return Error(bookmarks_errors::kEditBookmarksDisabled);

  std::optional<api::bookmarks::Create::Params> params =
      api::bookmarks::Create::Params::Create(args());
  if (!params)
    return BadMessage();

  std::string error;
  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  const BookmarkNode* node =
      CreateBookmarkNode(model, params->bookmark, &error);
  if (!node)
    return Error(error);

  BookmarkTreeNode ret = bookmark_api_helpers::GetBookmarkTreeNode(
      GetManagedBookmarkService(), node, false, false);
  return ArgumentList(api::bookmarks::Create::Results::Create(ret));
}

const BookmarkNode* BookmarksCreateFunction::CreateBookmarkNode(
    BookmarkModel* model,
    const CreateDetails& details,
    std::string* error) {
  int64_t parent_id;

  if (!details.parent_id) {
    // Optional, default to "other bookmarks".
    parent_id = model->other_node()->id();
  } else if (!base::StringToInt64(*details.parent_id, &parent_id)) {
    *error = bookmarks_errors::kInvalidIdError;
    return nullptr;
  }
  const BookmarkNode* parent = bookmarks::GetBookmarkNodeByID(model, parent_id);
  if (!CanBeModified(parent, error)) {
    return nullptr;
  }
  if (!parent->is_folder()) {
    *error = bookmarks_errors::kInvalidParentError;
    return nullptr;
  }

  size_t index;
  if (!details.index) {  // Optional (defaults to end).
    index = parent->children().size();
  } else {
    if (*details.index < 0 ||
        static_cast<size_t>(*details.index) > parent->children().size()) {
      *error = bookmarks_errors::kInvalidIndexError;
      return nullptr;
    }
    index = static_cast<size_t>(*details.index);
  }

  std::u16string title;  // Optional.
  if (details.title) {
    title = base::UTF8ToUTF16(*details.title);
  }

  std::string url_string;  // Optional.
  if (details.url) {
    url_string = *details.url;
  }

  GURL url(url_string);
  if (!url_string.empty() && !url.is_valid()) {
    *error = bookmarks_errors::kInvalidUrlError;
    return nullptr;
  }

  const BookmarkNode* node;
  if (url_string.length()) {
    node = model->AddNewURL(parent, index, title, url);
  } else {
    node = model->AddFolder(parent, index, title);
    model->SetDateFolderModified(parent, base::Time::Now());
  }

  DCHECK(node);

  return node;
}

ExtensionFunction::ResponseValue BookmarksMoveFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return Error(bookmarks_errors::kEditBookmarksDisabled);

  std::optional<api::bookmarks::Move::Params> params =
      api::bookmarks::Move::Params::Create(args());
  if (!params)
    return BadMessage();

  std::string error;
  const BookmarkNode* node = GetBookmarkNodeFromId(params->id, &error);
  if (!node)
    return Error(error);

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  if (model->is_permanent_node(node))
    return Error(bookmarks_errors::kModifySpecialError);

  const BookmarkNode* parent = nullptr;
  if (!params->destination.parent_id) {
    // Optional, defaults to current parent.
    parent = node->parent();
  } else {
    int64_t parent_id;
    if (!base::StringToInt64(*params->destination.parent_id, &parent_id))
      return Error(bookmarks_errors::kInvalidIdError);

    parent = bookmarks::GetBookmarkNodeByID(model, parent_id);
  }

  if (!CanBeModified(parent, &error) || !CanBeModified(node, &error)) {
    return Error(error);
  }

  if (!parent->is_folder()) {
    return Error(bookmarks_errors::kInvalidParentError);
  }

  if (parent->HasAncestor(node)) {
    return Error(bookmarks_errors::kInvalidMoveDestinationError);
  }

  size_t index;
  if (params->destination.index) {  // Optional (defaults to end).
    if (*params->destination.index < 0 ||
        static_cast<size_t>(*params->destination.index) >
            parent->children().size()) {
      return Error(bookmarks_errors::kInvalidIndexError);
    }
    index = static_cast<size_t>(*params->destination.index);
  } else {
    index = parent->children().size();
  }

  model->Move(node, parent, index);

  BookmarkTreeNode tree_node = bookmark_api_helpers::GetBookmarkTreeNode(
      GetManagedBookmarkService(), node, false, false);
  return ArgumentList(api::bookmarks::Move::Results::Create(tree_node));
}

ExtensionFunction::ResponseValue BookmarksUpdateFunction::RunOnReady() {
  if (!EditBookmarksEnabled())
    return Error(bookmarks_errors::kEditBookmarksDisabled);

  std::optional<api::bookmarks::Update::Params> params =
      api::bookmarks::Update::Params::Create(args());
  if (!params)
    return BadMessage();

  // Optional but we need to distinguish non present from an empty title.
  std::u16string title;
  bool has_title = false;
  if (params->changes.title) {
    title = base::UTF8ToUTF16(*params->changes.title);
    has_title = true;
  }

  // Optional.
  std::string url_string;
  if (params->changes.url)
    url_string = *params->changes.url;
  GURL url(url_string);
  if (!url_string.empty() && !url.is_valid())
    return Error(bookmarks_errors::kInvalidUrlError);

  std::string error;
  const BookmarkNode* node = GetBookmarkNodeFromId(params->id, &error);
  if (!CanBeModified(node, &error))
    return Error(error);

  BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(GetProfile());
  if (model->is_permanent_node(node))
    return Error(bookmarks_errors::kModifySpecialError);

  if (!url.is_empty() && node->is_folder())
    return Error(bookmarks_errors::kCannotSetUrlOfFolderError);

  if (has_title)
    model->SetTitle(node, title,
                    bookmarks::metrics::BookmarkEditSource::kExtension);
  if (!url.is_empty())
    model->SetURL(node, url,
                  bookmarks::metrics::BookmarkEditSource::kExtension);

  BookmarkTreeNode tree_node = bookmark_api_helpers::GetBookmarkTreeNode(
      GetManagedBookmarkService(), node, false, false);
  return ArgumentList(api::bookmarks::Update::Results::Create(tree_node));
}

}  // namespace extensions
