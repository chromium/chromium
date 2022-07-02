// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/bookmark_sync_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/sync_bookmarks/bookmark_sync_service.h"

// static
sync_bookmarks::BookmarkSyncService* BookmarkSyncServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<sync_bookmarks::BookmarkSyncService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BookmarkSyncServiceFactory* BookmarkSyncServiceFactory::GetInstance() {
  return base::Singleton<BookmarkSyncServiceFactory>::get();
}

BookmarkSyncServiceFactory::BookmarkSyncServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "BookmarkSyncServiceFactory",
          BrowserContextDependencyManager::GetInstance()) {
  DependsOn(BookmarkUndoServiceFactory::GetInstance());
}

BookmarkSyncServiceFactory::~BookmarkSyncServiceFactory() = default;

KeyedService* BookmarkSyncServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return new sync_bookmarks::BookmarkSyncService(
      BookmarkUndoServiceFactory::GetForProfileIfExists(profile));
}

content::BrowserContext* BookmarkSyncServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
