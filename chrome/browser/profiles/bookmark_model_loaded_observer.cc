// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/profiles/bookmark_model_loaded_observer.h"

#include "chrome/browser/sync/sync_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"

BookmarkModelLoadedObserver::BookmarkModelLoadedObserver(
    Profile* profile,
    bookmarks::BookmarkModel* model)
    : profile_(profile) {
  CHECK(model);
  observation_.Observe(model);
}

BookmarkModelLoadedObserver::~BookmarkModelLoadedObserver() = default;

void BookmarkModelLoadedObserver::BookmarkModelChanged() {
}

void BookmarkModelLoadedObserver::BookmarkModelLoaded(bool ids_reassigned) {
  // Causes lazy-load if sync is enabled.
  SyncServiceFactory::GetInstance()->GetForProfile(profile_);
  delete this;
}

void BookmarkModelLoadedObserver::BookmarkModelBeingDeleted() {
  delete this;
}
