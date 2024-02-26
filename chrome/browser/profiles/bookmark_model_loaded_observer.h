// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_BOOKMARK_MODEL_LOADED_OBSERVER_H_
#define CHROME_BROWSER_PROFILES_BOOKMARK_MODEL_LOADED_OBSERVER_H_

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"

class Profile;

namespace bookmarks {
class BookmarkModel;
}  // namespace bookmarks

class BookmarkModelLoadedObserver
    : public bookmarks::BaseBookmarkModelObserver {
 public:
  BookmarkModelLoadedObserver(Profile* profile,
                              bookmarks::BookmarkModel* model);
  BookmarkModelLoadedObserver(const BookmarkModelLoadedObserver&) = delete;
  ~BookmarkModelLoadedObserver() override;

  BookmarkModelLoadedObserver& operator=(const BookmarkModelLoadedObserver&) =
      delete;

 private:
  void BookmarkModelChanged() override;
  void BookmarkModelLoaded(bool ids_reassigned) override;
  void BookmarkModelBeingDeleted() override;

  const raw_ptr<Profile> profile_;
  base::ScopedObservation<bookmarks::BookmarkModel,
                          bookmarks::BaseBookmarkModelObserver>
      observation_{this};
};

#endif  // CHROME_BROWSER_PROFILES_BOOKMARK_MODEL_LOADED_OBSERVER_H_
