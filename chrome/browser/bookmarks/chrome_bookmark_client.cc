// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/chrome_bookmark_client.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/notreached.h"
#include "build/build_config.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_node.h"
#include "components/bookmarks/browser/bookmark_storage.h"
#include "components/bookmarks/common/bookmark_features.h"
#include "components/bookmarks/managed/managed_bookmark_service.h"
#include "components/bookmarks/managed/managed_bookmark_util.h"
#include "components/favicon/core/favicon_util.h"
#include "components/favicon_base/favicon_types.h"
#include "components/history/core/browser/history_service.h"
#include "components/history/core/browser/url_database.h"
#include "components/offline_pages/buildflags/buildflags.h"
#include "components/prefs/pref_service.h"
#include "components/sync/base/pref_names.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
#include "chrome/browser/offline_pages/offline_page_bookmark_observer.h"
#endif

ChromeBookmarkClient::ChromeBookmarkClient(
    Profile* profile,
    bookmarks::ManagedBookmarkService* managed_bookmark_service,
    sync_bookmarks::BookmarkSyncService* bookmark_sync_service)
    : profile_(profile),
      managed_bookmark_service_(managed_bookmark_service),
      bookmark_sync_service_(bookmark_sync_service) {
  if (!profile->IsOffTheRecord()) {
    PrefService* pref_service = profile->GetPrefs();
    base::UmaHistogramBoolean(
        "ReadingList.SyncStateMatchesBookmarks",
        pref_service->GetBoolean(syncer::prefs::kSyncReadingList) ==
            pref_service->GetBoolean(syncer::prefs::kSyncBookmarks));
  }
}

ChromeBookmarkClient::~ChromeBookmarkClient() {
}

void ChromeBookmarkClient::Init(bookmarks::BookmarkModel* model) {
  if (managed_bookmark_service_)
    managed_bookmark_service_->BookmarkModelCreated(model);
  model_ = model;

#if BUILDFLAG(ENABLE_OFFLINE_PAGES)
  offline_page_observer_ =
      std::make_unique<offline_pages::OfflinePageBookmarkObserver>(profile_);
  model->AddObserver(offline_page_observer_.get());
#endif
}

base::CancelableTaskTracker::TaskId
ChromeBookmarkClient::GetFaviconImageForPageURL(
    const GURL& page_url,
    favicon_base::FaviconImageCallback callback,
    base::CancelableTaskTracker* tracker) {
  return favicon::GetFaviconImageForPageURL(
      FaviconServiceFactory::GetForProfile(profile_,
                                           ServiceAccessType::EXPLICIT_ACCESS),
      page_url, favicon_base::IconType::kFavicon, std::move(callback), tracker);
}

bool ChromeBookmarkClient::SupportsTypedCountForUrls() {
  return true;
}

void ChromeBookmarkClient::GetTypedCountForUrls(
    UrlTypedCountMap* url_typed_count_map) {
  history::HistoryService* history_service =
      HistoryServiceFactory::GetForProfileIfExists(
          profile_, ServiceAccessType::EXPLICIT_ACCESS);
  history::URLDatabase* url_db =
      history_service ? history_service->InMemoryDatabase() : nullptr;
  if (!url_db)
    return;

  static const bool kTypedUrlsMapEnabled =
      base::FeatureList::IsEnabled(bookmarks::kTypedUrlsMap);
  if (kTypedUrlsMapEnabled) {
    // Create a map mapping each `GURL` to its typed count. This isn't cached
    // since the in memory DB is updated as the user navigates and updating this
    // as well seems like overkill.
    std::map<const GURL, int> typed_counts;
    history::URLDatabase::URLEnumerator history_enum;
    // Only need significant URLs, i.e. a superset of URls with >0 typed count,
    // since URLs not in `typed_counts` default to 0 typed count anyway.
    url_db->InitURLEnumeratorForSignificant(&history_enum);
    for (history::URLRow row; history_enum.GetNextURL(&row);) {
      if (row.typed_count() > 0)
        typed_counts[row.url()] = row.typed_count();
    }

    // Use the map to assign typed counts to `url_typed_count_map`.
    for (auto& url_typed_count_pair : *url_typed_count_map) {
      if (typed_counts.count(*url_typed_count_pair.first)) {
        url_typed_count_pair.second =
            typed_counts.count(*url_typed_count_pair.first);
      }
    }

  } else {
    for (auto& url_typed_count_pair : *url_typed_count_map) {
      // The in-memory URLDatabase might not cache all URLRows, but it
      // guarantees to contain those with `typed_count` > 0. Thus, if we cannot
      // fetch the URLRow, it is safe to assume that its `typed_count` is 0.
      history::URLRow url_row;
      const GURL* url = url_typed_count_pair.first;
      if (url && url_db->GetRowForURL(*url, &url_row))
        url_typed_count_pair.second = url_row.typed_count();
    }
  }
}

bool ChromeBookmarkClient::IsPermanentNodeVisibleWhenEmpty(
    bookmarks::BookmarkNode::Type type) {
#if BUILDFLAG(IS_ANDROID)
  const bool is_mobile = true;
#else
  const bool is_mobile = false;
#endif

  switch (type) {
    case bookmarks::BookmarkNode::URL:
      NOTREACHED();
      return false;
    case bookmarks::BookmarkNode::FOLDER:
      // Managed node.
      return false;
    case bookmarks::BookmarkNode::BOOKMARK_BAR:
    case bookmarks::BookmarkNode::OTHER_NODE:
      return !is_mobile;
    case bookmarks::BookmarkNode::MOBILE:
      return is_mobile;
  }

  return false;
}

void ChromeBookmarkClient::RecordAction(const base::UserMetricsAction& action) {
  base::RecordAction(action);
}

bookmarks::LoadManagedNodeCallback
ChromeBookmarkClient::GetLoadManagedNodeCallback() {
  if (!managed_bookmark_service_)
    return bookmarks::LoadManagedNodeCallback();

  return managed_bookmark_service_->GetLoadManagedNodeCallback();
}

bool ChromeBookmarkClient::CanSetPermanentNodeTitle(
    const bookmarks::BookmarkNode* permanent_node) {
  return !managed_bookmark_service_ ||
         managed_bookmark_service_->CanSetPermanentNodeTitle(permanent_node);
}

bool ChromeBookmarkClient::CanSyncNode(const bookmarks::BookmarkNode* node) {
  return !managed_bookmark_service_ ||
         managed_bookmark_service_->CanSyncNode(node);
}

bool ChromeBookmarkClient::CanBeEditedByUser(
    const bookmarks::BookmarkNode* node) {
  return !managed_bookmark_service_ ||
         managed_bookmark_service_->CanBeEditedByUser(node);
}

std::string ChromeBookmarkClient::EncodeBookmarkSyncMetadata() {
  return bookmark_sync_service_->EncodeBookmarkSyncMetadata();
}

void ChromeBookmarkClient::DecodeBookmarkSyncMetadata(
    const std::string& metadata_str,
    const base::RepeatingClosure& schedule_save_closure) {
  bookmark_sync_service_->DecodeBookmarkSyncMetadata(
      metadata_str, schedule_save_closure, model_);
}
