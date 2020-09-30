// Copyright 2020 The Chromium Authors. All rights reserved.
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
  const bookmarks::BookmarkNode* Add(const GURL& url,
                                     const std::string& title) override;
  const bookmarks::BookmarkNode* Get(const GURL& url) override;
  void Delete(const GURL& url) override;
  const bookmarks::BookmarkNode* GetRoot() const override;
  size_t size() const override;
  size_t unread_size() const override;
  void SetReadStatus(const GURL& url, bool read) override;
};

#endif  // CHROME_BROWSER_READING_LIST_ANDROID_EMPTY_READING_LIST_MANAGER_H_
