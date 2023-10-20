// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_LOCAL_OR_SYNCABLE_BOOKMARK_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_LOCAL_OR_SYNCABLE_BOOKMARK_SYNC_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace sync_bookmarks {
class BookmarkSyncService;
}  // namespace sync_bookmarks

// Singleton that builds BookmarkSyncService instances dealing with users that
// enable sync-the-feature (syncable profile bookmarks).
class LocalOrSyncableBookmarkSyncServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of BookmarkSyncService associated with this profile
  // (creating one if none exists).
  static sync_bookmarks::BookmarkSyncService* GetForProfile(Profile* profile);

  // Returns the instance of the LocalOrSyncableBookmarkSyncServiceFactory
  // singleton.
  static LocalOrSyncableBookmarkSyncServiceFactory* GetInstance();

  LocalOrSyncableBookmarkSyncServiceFactory(
      const LocalOrSyncableBookmarkSyncServiceFactory&) = delete;
  LocalOrSyncableBookmarkSyncServiceFactory& operator=(
      const LocalOrSyncableBookmarkSyncServiceFactory&) = delete;

 private:
  friend base::NoDestructor<LocalOrSyncableBookmarkSyncServiceFactory>;

  LocalOrSyncableBookmarkSyncServiceFactory();
  ~LocalOrSyncableBookmarkSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SYNC_LOCAL_OR_SYNCABLE_BOOKMARK_SYNC_SERVICE_FACTORY_H_
