// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_READING_LIST_ANDROID_EMPTY_READING_LIST_MANAGER_H_
#define CHROME_BROWSER_READING_LIST_ANDROID_EMPTY_READING_LIST_MANAGER_H_

#include "chrome/browser/reading_list/android/reading_list_manager.h"

// Empty implementation of ReadingListManager, used when the read later feature
// is disabled.
class EmptyReadingListManager : public ReadingListManager {
 public:
  EmptyReadingListManager();
  ~EmptyReadingListManager() override;

 private:
  // ReadingListManager implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  const bookmarks::BookmarkNode* Add(const GURL& url,
                                     const std::string& title) override;
  const bookmarks::BookmarkNode* Get(const GURL& url) const override;
  const bookmarks::BookmarkNode* GetNodeByID(int64_t id) const override;
  void GetMatchingNodes(
      const bookmarks::QueryFields& query,
      size_t max_count,
      std::vector<const bookmarks::BookmarkNode*>* nodes) override;
  bool IsReadingListBookmark(
      const bookmarks::BookmarkNode* node) const override;
  void Delete(const GURL& url) override;
  const bookmarks::BookmarkNode* GetRoot() const override;
  size_t size() const override;
  size_t unread_size() const override;
  void SetTitle(const GURL& url, const std::u16string& title) override;
  void SetReadStatus(const GURL& url, bool read) override;
  bool GetReadStatus(const bookmarks::BookmarkNode* node) override;
  bool IsLoaded() const override;
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_EMPTY_READING_LIST_MANAGER_H_
