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
#include "base/task/deferred_sequenced_task_runner.h"
#include "components/bookmarks/browser/bookmark_client.h"
#include "components/offline_pages/buildflags/buildflags.h"

class GURL;
class Profile;

namespace bookmarks {
class BookmarkModel;
class BookmarkNode;
class ManagedBookmarkService;
}

namespace sync_bookmarks {
class BookmarkSyncService;
}

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
namespace offline_pages {
class OfflinePageBookmarkObserver;
}
#endif

class ChromeBookmarkClient : public bookmarks::BookmarkClient {
 public:
  ChromeBookmarkClient(
      Profile* profile,
      bookmarks::ManagedBookmarkService* managed_bookmark_service,
      sync_bookmarks::BookmarkSyncService* bookmark_sync_service);

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
  bool IsPermanentNodeVisibleWhenEmpty(
      bookmarks::BookmarkNode::Type type) override;
  void RecordAction(const base::UserMetricsAction& action) override;
  bookmarks::LoadManagedNodeCallback GetLoadManagedNodeCallback() override;
  bool CanSetPermanentNodeTitle(
      const bookmarks::BookmarkNode* permanent_node) override;
  bool CanSyncNode(const bookmarks::BookmarkNode* node) override;
  bool CanBeEditedByUser(const bookmarks::BookmarkNode* node) override;
  std::string EncodeBookmarkSyncMetadata() override;
  void DecodeBookmarkSyncMetadata(
      const std::string& metadata_str,
      const base::RepeatingClosure& schedule_save_closure) override;

 private:
  // Pointer to the associated Profile. Must outlive ChromeBookmarkClient.
  raw_ptr<Profile> profile_;

  // Pointer to the ManagedBookmarkService responsible for bookmark policy. May
  // be null during testing.
  raw_ptr<bookmarks::ManagedBookmarkService> managed_bookmark_service_;

  raw_ptr<bookmarks::BookmarkModel> model_;

  // Pointer to the BookmarkSyncService responsible for encoding and decoding
  // sync metadata persisted together with the bookmarks model.
  raw_ptr<sync_bookmarks::BookmarkSyncService> bookmark_sync_service_;

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  // Owns the observer used by Offline Page listening to Bookmark Model events.
  std::unique_ptr<offline_pages::OfflinePageBookmarkObserver>
      offline_page_observer_;
#endif
};

#endif  // CHROME_BROWSER_BOOKMARKS_CHROME_BOOKMARK_CLIENT_H_
