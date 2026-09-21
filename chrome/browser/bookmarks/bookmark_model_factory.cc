// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_model_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/bookmarks/chrome_bookmark_client.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/sync/account_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/bookmarks/browser/bookmark_utils.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"
#include "components/undo/bookmark_undo_service.h"

namespace {

using bookmarks::BookmarkModel;

std::unique_ptr<KeyedService> BuildBookmarkModel(
    content::BrowserContext* context) {
  Profile* profile = Profile::FromBrowserContext(context);
  auto bookmark_model =
      std::make_unique<BookmarkModel>(std::make_unique<ChromeBookmarkClient>(
          profile, ManagedBookmarkServiceFactory::GetForProfile(profile),
          LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(profile),
          AccountBookmarkSyncServiceFactory::GetForProfile(profile),
          BookmarkUndoServiceFactory::GetForProfile(profile)));
  bookmark_model->Load(profile->GetPath());
  BookmarkUndoServiceFactory::GetForProfile(profile)
      ->StartObservingBookmarkModel(bookmark_model.get());
  return bookmark_model;
}

}  // namespace

// static
BookmarkModel* BookmarkModelFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<BookmarkModel*>(
      GetInstance()->GetServiceForBrowserContext(context, /*create=*/true));
}

// static
BookmarkModel* BookmarkModelFactory::GetForBrowserContextIfExists(
    content::BrowserContext* context) {
  return static_cast<BookmarkModel*>(
      GetInstance()->GetServiceForBrowserContext(context, false));
}

// static
BookmarkModelFactory* BookmarkModelFactory::GetInstance() {
  static base::NoDestructor<BookmarkModelFactory> instance;
  return instance.get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
BookmarkModelFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildBookmarkModel);
}

BookmarkModelFactory::BookmarkModelFactory()
    : ProfileKeyedServiceFactory(
          "BookmarkModel",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Use OTR profile for Guest session.
              // (Bookmarks can be enabled in Guest sessions under some
              // enterprise policies.)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // No service for system profile.
              .WithSystem(ProfileSelection::kNone)
              // ChromeOS creates various profiles (login, lock screen...) that
              // do not have/need access to bookmarks.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(AccountBookmarkSyncServiceFactory::GetInstance());
  DependsOn(BookmarkUndoServiceFactory::GetInstance());
  DependsOn(ManagedBookmarkServiceFactory::GetInstance());
  DependsOn(LocalOrSyncableBookmarkSyncServiceFactory::GetInstance());
}

BookmarkModelFactory::~BookmarkModelFactory() = default;

std::unique_ptr<KeyedService>
BookmarkModelFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildBookmarkModel(context);
}

void BookmarkModelFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  bookmarks::RegisterProfilePrefs(registry);
}

bool BookmarkModelFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
