// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_H_
#define CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_H_

#include <string>

#include "components/keyed_service/core/keyed_service.h"

namespace bookmarks {
class BookmarkNode;
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

  // Adds a reading list article to the unread section, and return the bookmark
  // node representation. The bookmark node is owned by this class. If there is
  // a duplicate URL, swaps the current reading list item. Returns nullptr on
  // failure.
  virtual const bookmarks::BookmarkNode* Add(const GURL& url,
                                             const std::string& title) = 0;

  // Gets the bookmark node representation of a reading list article. The
  // bookmark node is owned by this class. Returns nullptr if no such reading
  // list.
  virtual const bookmarks::BookmarkNode* Get(const GURL& url) = 0;

  // Deletes a reading list article.
  virtual void Delete(const GURL& url) = 0;

  // Returns the root bookmark node for the reading list article. The bookmark
  // node tree is owned by this class. All reading list articles are children of
  // this root.
  virtual const bookmarks::BookmarkNode* GetRoot() const = 0;

  // Returns the total number of reading list articles. This doesn't include the
  // bookmark root.
  virtual size_t size() const = 0;

  // Returns the total number of unread articles.
  virtual size_t unread_size() const = 0;

  // Sets the read status for a reading list article. No op if such reading list
  // article doesn't exist.
  virtual void SetReadStatus(const GURL& url, bool read) = 0;
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_READING_LIST_MANAGER_H_
