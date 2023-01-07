// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BOOKMARK_MODEL_LOADED_OBSERVER_H_
#define CHROME_BROWSER_PROFILES_BOOKMARK_MODEL_LOADED_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"

class Profile;

class BookmarkModelLoadedObserver
    : public bookmarks::BaseBookmarkModelObserver {
 public:
  explicit BookmarkModelLoadedObserver(Profile* profile);

  BookmarkModelLoadedObserver(const BookmarkModelLoadedObserver&) = delete;
  BookmarkModelLoadedObserver& operator=(const BookmarkModelLoadedObserver&) =
      delete;

 private:
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bookmarks::BookmarkModel* model,
                           bool ids_reassigned) override;
  void BookmarkModelBeingDeleted(bookmarks::BookmarkModel* model) override;

  raw_ptr<Profile> profile_;
};

#endif  // CHROME_BROWSER_PROFILES_BOOKMARK_MODEL_LOADED_OBSERVER_H_
