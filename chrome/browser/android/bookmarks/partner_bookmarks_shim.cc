// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/bookmarks/partner_bookmarks_shim.h"

#include <tuple>
#include <utility>

#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/lazy_instance.h"
#include "base/memory/ptr_util.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/android/bookmarks/partner_bookmarks_reader.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/url_formatter.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/models/tree_node_iterator.h"

using bookmarks::BookmarkNode;
using content::BrowserThread;

namespace {

// PartnerModelKeeper is used as a singleton to store an immutable hierarchy
// of partner bookmarks.  The hierarchy is retrieved from the partner bookmarks
// provider and doesn't depend on the user profile.
// The retrieved hierarchy persists
// PartnerBookmarksShim is responsible to applying and storing the user changes
// (deletions/renames) in the user profile, thus keeping the hierarchy intact.
struct PartnerModelKeeper {
  std::unique_ptr<BookmarkNode> partner_bookmarks_root;
  bool loaded;

  PartnerModelKeeper()
    : loaded(false) {}
};

base::LazyInstance<PartnerModelKeeper>::DestructorAtExit
    g_partner_model_keeper = LAZY_INSTANCE_INITIALIZER;

const void* const kPartnerBookmarksShimUserDataKey =
    &kPartnerBookmarksShimUserDataKey;

// Dictionary keys for entries in the kPartnerBookmarksMapping pref.
const char kMappingUrl[] = "url";
const char kMappingProviderTitle[] = "provider_title";
const char kMappingTitle[] = "mapped_title";

bool g_disable_partner_bookmarks_editing = false;

}  // namespace

// static
PartnerBookmarksShim* PartnerBookmarksShim::BuildForBrowserContext(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  PartnerBookmarksShim* data =
      static_cast<PartnerBookmarksShim*>(
          browser_context->GetUserData(kPartnerBookmarksShimUserDataKey));
  if (data)
    return data;

  data = new PartnerBookmarksShim(
      Profile::FromBrowserContext(browser_context)->GetPrefs());
  browser_context->SetUserData(kPartnerBookmarksShimUserDataKey,
                               base::WrapUnique(data));
  data->ReloadNodeMapping();
  return data;
}

// static
void PartnerBookmarksShim::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterListPref(prefs::kPartnerBookmarkMappings);
}

// static
void PartnerBookmarksShim::DisablePartnerBookmarksEditing() {
  g_disable_partner_bookmarks_editing = true;
}

bool PartnerBookmarksShim::IsLoaded() const {
  return g_partner_model_keeper.Get().loaded;
}

bool PartnerBookmarksShim::HasPartnerBookmarks() const {
  DCHECK(IsLoaded());
  return g_partner_model_keeper.Get().partner_bookmarks_root.get() != NULL;
}

bool PartnerBookmarksShim::IsReachable(const BookmarkNode* node) const {
  DCHECK(IsPartnerBookmark(node));
  if (!HasPartnerBookmarks())
    return false;
  if (!g_disable_partner_bookmarks_editing) {
    for (const BookmarkNode* i = node; i != NULL; i = i->parent()) {
      const NodeRenamingMapKey key(i->url(), i->GetTitle());
      NodeRenamingMap::const_iterator remap = node_rename_remove_map_.find(key);
      if (remap != node_rename_remove_map_.end() && remap->second.empty())
        return false;
    }
  }
  return true;
}

bool PartnerBookmarksShim::IsEditable(const BookmarkNode* node) const {
  DCHECK(IsPartnerBookmark(node));
  if (!HasPartnerBookmarks())
    return false;
  if (g_disable_partner_bookmarks_editing)
    return false;
  return true;
}

void PartnerBookmarksShim::RemoveBookmark(const BookmarkNode* node) {
  DCHECK(IsEditable(node));
  RenameBookmark(node, std::u16string());
}

void PartnerBookmarksShim::RenameBookmark(const BookmarkNode* node,
                                          const std::u16string& title) {
  DCHECK(IsEditable(node));
  const NodeRenamingMapKey key(node->url(), node->GetTitle());
  node_rename_remove_map_[key] = title;
  SaveNodeMapping();
  for (PartnerBookmarksShim::Observer& observer : observers_)
    observer.PartnerShimChanged(this);
}

void PartnerBookmarksShim::AddObserver(
    PartnerBookmarksShim::Observer* observer) {
  observers_.AddObserver(observer);
}

void PartnerBookmarksShim::RemoveObserver(
    PartnerBookmarksShim::Observer* observer) {
  observers_.RemoveObserver(observer);
}

const BookmarkNode* PartnerBookmarksShim::GetNodeByID(int64_t id) const {
  DCHECK(IsLoaded());
  if (!HasPartnerBookmarks())
    return NULL;
  return GetNodeByID(GetPartnerBookmarksRoot(), id);
}

std::u16string PartnerBookmarksShim::GetTitle(const BookmarkNode* node) const {
  DCHECK(node);
  DCHECK(IsPartnerBookmark(node));

  if (!g_disable_partner_bookmarks_editing) {
    const NodeRenamingMapKey key(node->url(), node->GetTitle());
    NodeRenamingMap::const_iterator i = node_rename_remove_map_.find(key);
    if (i != node_rename_remove_map_.end())
      return i->second;
  }

  return node->GetTitle();
}

bool PartnerBookmarksShim::IsPartnerBookmark(const BookmarkNode* node) const {
  DCHECK(IsLoaded());
  if (!HasPartnerBookmarks())
    return false;
  const BookmarkNode* parent = node;
  while (parent) {
    if (parent == GetPartnerBookmarksRoot())
      return true;
    parent = parent->parent();
  }
  return false;
}

const BookmarkNode* PartnerBookmarksShim::GetPartnerBookmarksRoot() const {
  return g_partner_model_keeper.Get().partner_bookmarks_root.get();
}

void PartnerBookmarksShim::SetPartnerBookmarksRoot(
    std::unique_ptr<bookmarks::BookmarkNode> root_node) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  g_partner_model_keeper.Get().partner_bookmarks_root = std::move(root_node);
  g_partner_model_keeper.Get().loaded = true;
  for (PartnerBookmarksShim::Observer& observer : observers_)
    observer.PartnerShimLoaded(this);
}

PartnerBookmarksShim::NodeRenamingMapKey::NodeRenamingMapKey(
    const GURL& url,
    const std::u16string& provider_title)
    : url_(url), provider_title_(provider_title) {}

PartnerBookmarksShim::NodeRenamingMapKey::~NodeRenamingMapKey() {}

bool operator<(const PartnerBookmarksShim::NodeRenamingMapKey& a,
               const PartnerBookmarksShim::NodeRenamingMapKey& b) {
  return std::tie(a.url_, a.provider_title_) <
         std::tie(b.url_, b.provider_title_);
}

// static
void PartnerBookmarksShim::ClearInBrowserContextForTesting(
    content::BrowserContext* browser_context) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  browser_context->SetUserData(kPartnerBookmarksShimUserDataKey, 0);
}

// static
void PartnerBookmarksShim::ClearPartnerModelForTesting() {
  g_partner_model_keeper.Get().loaded = false;
  g_partner_model_keeper.Get().partner_bookmarks_root.reset(0);
}

// static
void PartnerBookmarksShim::EnablePartnerBookmarksEditing() {
  g_disable_partner_bookmarks_editing = false;
}

PartnerBookmarksShim::PartnerBookmarksShim(PrefService* prefs)
    : prefs_(prefs), observers_(base::ObserverListPolicy::EXISTING_ONLY) {}

PartnerBookmarksShim::~PartnerBookmarksShim() {
  for (PartnerBookmarksShim::Observer& observer : observers_)
    observer.ShimBeingDeleted(this);
}

const BookmarkNode* PartnerBookmarksShim::GetNodeByID(
    const BookmarkNode* parent,
    int64_t id) const {
  if (parent->id() == id)
    return parent;
  for (const auto& node : parent->children()) {
    const BookmarkNode* result = GetNodeByID(node.get(), id);
    if (result)
      return result;
  }
  return nullptr;
}

void PartnerBookmarksShim::ReloadNodeMapping() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  node_rename_remove_map_.clear();
  if (!prefs_)
    return;

  const base::Value::List& list =
      prefs_->GetList(prefs::kPartnerBookmarkMappings);

  for (const auto& entry : list) {
    if (!entry.is_dict()) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }
    const base::Value::Dict& dict = entry.GetDict();

    const std::string* url = dict.FindString(kMappingUrl);
    const std::string* provider_title = dict.FindString(kMappingProviderTitle);
    const std::string* mapped_title = dict.FindString(kMappingTitle);
    if (!url || !provider_title || !mapped_title) {
      NOTREACHED_IN_MIGRATION();
      continue;
    }

    const NodeRenamingMapKey key(GURL(*url),
                                 base::UTF8ToUTF16(*provider_title));
    node_rename_remove_map_[key] = base::UTF8ToUTF16(*mapped_title);
  }
}

void PartnerBookmarksShim::SaveNodeMapping() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  if (!prefs_)
    return;

  base::Value::List list;
  for (NodeRenamingMap::const_iterator i = node_rename_remove_map_.begin();
       i != node_rename_remove_map_.end();
       ++i) {
    base::Value::Dict dict;
    dict.Set(kMappingUrl, i->first.url().spec());
    dict.Set(kMappingProviderTitle, i->first.provider_title());
    dict.Set(kMappingTitle, i->second);
    list.Append(std::move(dict));
  }
  prefs_->SetList(prefs::kPartnerBookmarkMappings, std::move(list));
}

void PartnerBookmarksShim::GetPartnerBookmarksMatchingProperties(
    const bookmarks::QueryFields& query,
    size_t max_count,
    std::vector<const BookmarkNode*>* nodes) {
  DCHECK(nodes->size() <= max_count);

  std::vector<std::u16string> query_words =
      bookmarks::ParseBookmarkQuery(query);
  if (query_words.empty())
    return;
  ui::TreeNodeIterator<const BookmarkNode> iterator(GetPartnerBookmarksRoot());
  // The check that size < max_count is necessary because we will search for
  // user (non-partner) bookmarks before calling this function
  while (iterator.has_next() && nodes->size() < max_count) {
    const BookmarkNode* node = iterator.Next();
    // Make sure we don't include the "Partner Bookmarks" folder
    if (node == GetPartnerBookmarksRoot())
      continue;
    if (!query_words.empty() && !bookmarks::DoesBookmarkContainWords(
                                    GetTitle(node), node->url(), query_words))
      continue;
    if (query.title && GetTitle(node) != *query.title)
      continue;
    nodes->push_back(node);
  }
}
