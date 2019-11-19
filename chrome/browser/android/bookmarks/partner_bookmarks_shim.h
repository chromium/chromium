// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_SHIM_H_
#define CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_SHIM_H_

#include <stdint.h>

#include <memory>

#include "base/android/jni_weak_ref.h"
#include "base/i18n/case_conversion.h"
#include "base/i18n/string_search.h"
#include "base/macros.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/supports_user_data.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "ui/base/models/tree_node_iterator.h"
#include "url/gurl.h"

class PrefService;

namespace content {
class BrowserContext;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// A shim that lives on top of a BookmarkModel that allows the injection of
// partner bookmarks without submitting changes to the bookmark model.
// The shim persists bookmark renames/deletions in a user profile and could be
// queried via shim->GetTitle(node) and shim->IsReachable(node).
// Note that node->GetTitle() returns an original (unmodified) title.
class PartnerBookmarksShim : public base::SupportsUserData::Data {
 public:
  ~PartnerBookmarksShim() override;

  // Returns an instance of the shim for a given |browser_context|.
  static PartnerBookmarksShim* BuildForBrowserContext(
      content::BrowserContext* browser_context);

  // Registers preferences.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

  // Disables the editing and stops any edits from being applied.
  // The user will start to see the original (unedited) partner bookmarks.
  // Edits are stored in the user profile, so once the editing is enabled
  // ("not disabled") the user would see the edited partner bookmarks.
  // This method should be called as early as possible: it does NOT send any
  // notifications to already existing shims.
  static void DisablePartnerBookmarksEditing();

  // Returns true if everything got loaded.
  bool IsLoaded() const;

  // Returns true if there are partner bookmarks.
  bool HasPartnerBookmarks() const;

  // Returns true if a given bookmark is reachable (i.e. neither the bookmark,
  // nor any of its parents were "removed").
  bool IsReachable(const bookmarks::BookmarkNode* node) const;

  // Returns true if a given node is editable and if editing is allowed.
  bool IsEditable(const bookmarks::BookmarkNode* node) const;

  // Removes a given bookmark.
  // Makes the |node| (and, consequently, all its children) unreachable.
  void RemoveBookmark(const bookmarks::BookmarkNode* node);

  // Renames a given bookmark.
  void RenameBookmark(const bookmarks::BookmarkNode* node,
                      const base::string16& title);

  // For Loaded/Changed/ShimBeingDeleted notifications
  class Observer {
   public:
    // Called when the set of bookmarks, or their values/visibility changes
    virtual void PartnerShimChanged(PartnerBookmarksShim*) {}
    // Called when everything is loaded
    virtual void PartnerShimLoaded(PartnerBookmarksShim*) {}
    // Called just before everything got destroyed
    virtual void ShimBeingDeleted(PartnerBookmarksShim*) {}
   protected:
    virtual ~Observer() {}
  };

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // PartnerBookmarksShim versions of BookmarkModel/BookmarkNode methods
  const bookmarks::BookmarkNode* GetNodeByID(int64_t id) const;
  base::string16 GetTitle(const bookmarks::BookmarkNode* node) const;

  bool IsPartnerBookmark(const bookmarks::BookmarkNode* node) const;
  const bookmarks::BookmarkNode* GetPartnerBookmarksRoot() const;

  // Sets the root node of the partner bookmarks and notifies any observers that
  // the shim has now been loaded.
  void SetPartnerBookmarksRoot(
      std::unique_ptr<bookmarks::BookmarkNode> root_node);

  // Used as a "unique" identifier of the partner bookmark node for the purposes
  // of node deletion and title editing. Two bookmarks with the same URLs and
  // titles are considered indistinguishable.
  class NodeRenamingMapKey {
   public:
    NodeRenamingMapKey(const GURL& url, const base::string16& provider_title);
    ~NodeRenamingMapKey();
    const GURL& url() const { return url_; }
    const base::string16& provider_title() const { return provider_title_; }
    friend bool operator<(const NodeRenamingMapKey& a,
                          const NodeRenamingMapKey& b);
   private:
    GURL url_;
    base::string16 provider_title_;
  };
  typedef std::map<NodeRenamingMapKey, base::string16> NodeRenamingMap;

  // PartnerBookmarksShim version of BookmarkUtils methods
  void GetPartnerBookmarksMatchingProperties(
      const bookmarks::QueryFields& query,
      size_t max_count,
      std::vector<const bookmarks::BookmarkNode*>* nodes);

  // For testing: clears an instance of the shim in a given |browser_context|.
  static void ClearInBrowserContextForTesting(
      content::BrowserContext* browser_context);

  // For testing: clears partner bookmark model data.
  static void ClearPartnerModelForTesting();

  // For testing: re-enables partner bookmarks editing.
  static void EnablePartnerBookmarksEditing();

 private:
  explicit PartnerBookmarksShim(PrefService* prefs);

  const bookmarks::BookmarkNode* GetNodeByID(
      const bookmarks::BookmarkNode* parent,
      int64_t id) const;
  void ReloadNodeMapping();
  void SaveNodeMapping();

  std::unique_ptr<bookmarks::BookmarkNode> partner_bookmarks_root_;
  PrefService* prefs_;
  NodeRenamingMap node_rename_remove_map_;

  // The observers.
  base::ObserverList<Observer>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(PartnerBookmarksShim);
};

#endif  // CHROME_BROWSER_ANDROID_BOOKMARKS_PARTNER_BOOKMARKS_SHIM_H_
