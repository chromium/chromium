// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "components/sync/model/wipe_model_upon_sync_disabled_behavior.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"

// static
sync_bookmarks::BookmarkSyncService*
LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<sync_bookmarks::BookmarkSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
LocalOrSyncableBookmarkSyncServiceFactory*
LocalOrSyncableBookmarkSyncServiceFactory::GetInstance() {
  static base::NoDestructor<LocalOrSyncableBookmarkSyncServiceFactory> instance;
  return instance.get();
}

LocalOrSyncableBookmarkSyncServiceFactory::
    LocalOrSyncableBookmarkSyncServiceFactory()
    : ProfileKeyedServiceFactory(
          "LocalOrSyncableBookmarkSyncServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Bookmarks can be enabled in Guest sessions under some
              // enterprise policies, see BookmarkModelFactory. Sync
              // isn't used in guest, but as a dependency for
              // BookmarkModelFactory it is necessary to instantiate
              // BookmarkSyncService too (although it doesn't do anything
              // useful).
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

LocalOrSyncableBookmarkSyncServiceFactory::
    ~LocalOrSyncableBookmarkSyncServiceFactory() = default;

std::unique_ptr<KeyedService> LocalOrSyncableBookmarkSyncServiceFactory::
    BuildServiceInstanceForBrowserContext(
        content::BrowserContext* context) const {
  return std::make_unique<sync_bookmarks::BookmarkSyncService>(
      syncer::WipeModelUponSyncDisabledBehavior::kNever);
}
