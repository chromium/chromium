// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_
#define CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/task/deferred_sequenced_task_runner.h"
#include "components/bookmarks/browser/base_bookmark_model_observer.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/power_bookmarks/core/bookmark_client_base.h"

class BookmarkUndoService;
class GURL;
class Profile;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
class ManagedBookmarkService;
}  // namespace bookmarks

namespace sync_bookmarks {
class BookmarkSyncService;
}  // namespace sync_bookmarks

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
namespace offline_pages {
class OfflinePageBookmarkObserver;
}  // namespace offline_pages
#endif

namespace power_bookmarks {
class SuggestedSaveLocationProvider;
}

class ChromeBookmarkClient : public power_bookmarks::BookmarkClientBase {
 public:
  ChromeBookmarkClient(
      Profile* profile,
      bookmarks::ManagedBookmarkService* managed_bookmark_service,
      sync_bookmarks::BookmarkSyncService*
          local_or_syncable_bookmark_sync_service,
      sync_bookmarks::BookmarkSyncService* account_bookmark_sync_service,
      BookmarkUndoService* bookmark_undo_service);

  ChromeBookmarkClient(const ChromeBookmarkClient&) = delete;
  ChromeBookmarkClient& operator=(const ChromeBookmarkClient&) = delete;

  ~ChromeBookmarkClient() override;

  // bookmarks::BookmarkClient:
  void Init(bookmarks::BookmarkModel* model) override;
  base::CancelableTaskTracker::TaskId GetFaviconImageForPageURL(
      const GURL& page_url,
      favicon_base::FaviconImageCallback callback,
      base::CancelableTaskTracker* tracker) override;
  bool SupportsTypedCountForUrls() override;
  void GetTypedCountForUrls(UrlTypedCountMap* url_typed_count_map) override;
  bookmarks::LoadManagedNodeCallback GetLoadManagedNodeCallback() override;
  bool IsSyncFeatureEnabledIncludingBookmarks() override;
  bool CanSetPermanentNodeTitle(
      const bookmarks::BookmarkNode* permanent_node) override;
  bool IsNodeManaged(const bookmarks::BookmarkNode* node) override;
  std::string EncodeLocalOrSyncableBookmarkSyncMetadata() override;
  std::string EncodeAccountBookmarkSyncMetadata() override;
  void DecodeLocalOrSyncableBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override;
  void DecodeAccountBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override;
  void OnBookmarkNodeRemovedUndoable(
      const bookmarks::BookmarkNode* parent,
      size_t index,
      std::unique_ptr<bookmarks::BookmarkNode> node) override;

 private:
  // Pointer to the associated Profile. Must outlive ChromeBookmarkClient.
  const raw_ptr<Profile> profile_;

  // Pointer to the ManagedBookmarkService responsible for bookmark policy. May
  // be null during testing.
  const raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;

  // Pointers to the two BookmarkSyncService instances responsible for encoding
  // and decoding sync metadata persisted together with the bookmarks model.
  const raw_ptr<sync_bookmarks::BookmarkSyncService>
      local_or_syncable_bookmark_sync_service_;
  const raw_ptr<sync_bookmarks::BookmarkSyncService>
      account_bookmark_sync_service_;

  // Pointer to BookmarkUndoService, responsible for making operations undoable.
  const raw_ptr<BookmarkUndoService> bookmark_undo_service_;

  raw_ptr<bookmarks::BookmarkModel> model_ = nullptr;

  std::unique_ptr<power_bookmarks::SuggestedSaveLocationProvider>
      shopping_save_location_provider_;

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // Owns the observer used by Offline Page listening to Bookmark Model events.
  std::unique_ptr<offline_pages::OfflinePageBookmarkObserver>
      offline_page_observer_;

  // Observation of this by the bookmark model.
  std::unique_ptr<base::ScopedObservation<bookmarks::BookmarkModel,
                                          bookmarks::BaseBookmarkModelObserver>>
      model_observation_{};

#endif
};

#endif  // CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_
