// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/desktop_forcing_chrome_bookmark_test_client.h"

#include "base/functional/bind.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/account_bookmark_sync_service_factory.h"
#include "chrome/browser/sync/local_or_syncable_bookmark_sync_service_factory.h"
#include "chrome/browser/undo/bookmark_undo_service_factory.h"
#include "components/bookmarks/browser/bookmark_form_factor.h"
#include "components/bookmarks/browser/bookmark_model.h"
#include "components/undo/bookmark_undo_service.h"

bookmarks::BookmarkFormFactor
DesktopForcingChromeBookmarkTestClient::GetBookmarkFormFactor() {
  return bookmarks::BookmarkFormFactor::kDesktop;
}

BrowserContextKeyedServiceFactory::TestingFactory
DesktopForcingChromeBookmarkTestClient::GetTestingFactory() {
  return base::BindRepeating([](content::BrowserContext* context)
                                 -> std::unique_ptr<KeyedService> {
    Profile* profile = Profile::FromBrowserContext(context);
    auto bookmark_model = std::make_unique<bookmarks::BookmarkModel>(
        std::make_unique<DesktopForcingChromeBookmarkTestClient>(
            profile, ManagedBookmarkServiceFactory::GetForProfile(profile),
            LocalOrSyncableBookmarkSyncServiceFactory::GetForProfile(profile),
            AccountBookmarkSyncServiceFactory::GetForProfile(profile),
            BookmarkUndoServiceFactory::GetForProfile(profile)));
    bookmark_model->Load(profile->GetPath());
    BookmarkUndoServiceFactory::GetForProfile(profile)
        ->StartObservingBookmarkModel(bookmark_model.get());
    return bookmark_model;
  });
}
