// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_H_
#define CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_H_

#include <string>

#include "base/observer_list_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace bookmarks {
class BookmarkNode;
struct QueryFields;
}  // namespace bookmarks

class GURL;

// Owns a reading list model and converts reading list data to bookmark nodes.
// The bookmark nodes won't be persisted across sessions.
class ReadingListManager : public KeyedService {
 public:
  ReadingListManager() = default;
  ~ReadingListManager() override = default;

  ReadingListManager(const ReadingListManager&) = delete;
  ReadingListManager& operator=(const ReadingListManager&) = delete;

  // The observer that listens to reading list manager events.
  class Observer : public base::CheckedObserver {
   public:
    // Called when the reading list backend is loaded.
    virtual void ReadingListLoaded() {}

    // Called when the reading list backend is changed.
    virtual void ReadingListChanged() {}

    Observer() = default;
    ~Observer() override = default;
  };

  // Adds/Removes observers.
  virtual void AddObserver(Observer* observer) = 0;
  virtual void RemoveObserver(Observer* observer) = 0;

  // Adds a reading list article to the unread section, and return the bookmark
  // node representation. The bookmark node is owned by this class. If there is
  // a duplicate URL, a new bookmark node will be created, and the old bookmark
  // node pointer will be invalidated. May return nullptr on error.
  virtual const bookmarks::BookmarkNode* Add(const GURL& url,
                                             const std::string& title) = 0;

  // Gets the bookmark node representation of a reading list article. The
  // bookmark node is owned by this class. Returns nullptr if no such reading
  // list.
  virtual const bookmarks::BookmarkNode* Get(const GURL& url) const = 0;

  // Gets the bookmark node for the given |id|. The returned node can be the
  // root folder node. Returns nullptr if no match.
  virtual const bookmarks::BookmarkNode* GetNodeByID(int64_t id) const = 0;

  // Gets the bookmark nodes that match a search |query|. The resulting nodes
  // are appended to the |results| up to a upper limit of |max_count|. No-op if
  // |results| already have |max_count| nodes.
  virtual void GetMatchingNodes(
      const bookmarks::QueryFields& query,
      size_t max_count,
      std::vector<const bookmarks::BookmarkNode*>* results) = 0;

  // Returns whether the bookmark node is maintained in reading list manager.
  // Will return true if |node| is the root for reading list nodes.
  virtual bool IsReadingListBookmark(
      const bookmarks::BookmarkNode* node) const = 0;

  // Deletes a reading list article.
  virtual void Delete(const GURL& url) = 0;

  // Deletes all reading list articles.
  virtual void DeleteAll() = 0;

  // Returns the root bookmark node for the reading list article. The bookmark
  // node tree is owned by this class. All reading list articles are children of
  // this root.
  virtual const bookmarks::BookmarkNode* GetRoot() const = 0;

  // Returns the total number of reading list articles. This doesn't include the
  // bookmark root.
  virtual size_t size() const = 0;

  // Returns the total number of unread articles.
  virtual size_t unread_size() const = 0;

  // Sets the title for a reading list article. No op if such reading list
  // article doesn't exist.
  virtual void SetTitle(const GURL& url, const std::u16string& title) = 0;

  // Sets the read status for a reading list article. No op if such reading list
  // article doesn't exist.
  virtual void SetReadStatus(const GURL& url, bool read) = 0;

  // Gets the read status for a reading list article. The |node| must exist in
  // the reading list.
  virtual bool GetReadStatus(const bookmarks::BookmarkNode* node) = 0;

  // Returns whether the reading list manager is loaded.
  virtual bool IsLoaded() const = 0;
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_H_
