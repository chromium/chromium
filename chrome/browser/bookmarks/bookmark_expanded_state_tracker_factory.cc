// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker_factory.h"

#include <memory>

#include "chrome/browser/bookmarks/bookmark_expanded_state_tracker.h"
#include "chrome/browser/profiles/profile.h"
#include "components/bookmarks/common/bookmark_pref_names.h"
#include "components/pref_registry/pref_registry_syncable.h"

// static
BookmarkExpandedStateTracker*
BookmarkExpandedStateTrackerFactory::GetForProfile(Profile* profile) {
  return static_cast<BookmarkExpandedStateTracker*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BookmarkExpandedStateTrackerFactory*
BookmarkExpandedStateTrackerFactory::GetInstance() {
  static base::NoDestructor<BookmarkExpandedStateTrackerFactory> instance;
  return instance.get();
}

BookmarkExpandedStateTrackerFactory::BookmarkExpandedStateTrackerFactory()
    : ProfileKeyedServiceFactory(
          "BookmarkExpandedStateTracker",
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
              .Build()) {}

BookmarkExpandedStateTrackerFactory::~BookmarkExpandedStateTrackerFactory() =
    default;

void BookmarkExpandedStateTrackerFactory::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  // Don't sync this, as otherwise, due to a limitation in sync, it
  // will cause a deadlock (see http://crbug.com/97955).  If we truly
  // want to sync the expanded state of folders, it should be part of
  // bookmark sync itself (i.e., a property of the sync folder nodes).
  registry->RegisterListPref(bookmarks::prefs::kBookmarkEditorExpandedNodes);
}

std::unique_ptr<KeyedService>
BookmarkExpandedStateTrackerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<BookmarkExpandedStateTracker>(profile->GetPrefs());
}
