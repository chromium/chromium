// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bookmarks/bookmark_merged_surface_service_factory.h"

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/bookmarks/bookmark_merged_surface_service.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/bookmarks/managed_bookmark_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace {
std::unique_ptr<KeyedService> BuildBookmarkMergedSurfaceService(
    content::BrowserContext* context) {
  return std::make_unique<BookmarkMergedSurfaceService>(
      BookmarkModelFactory::GetInstance()->GetForBrowserContext(context),
      ManagedBookmarkServiceFactory::GetForProfile(
          Profile::FromBrowserContext(context)));
}
}  // namespace

// static
BookmarkMergedSurfaceService*
BookmarkMergedSurfaceServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<BookmarkMergedSurfaceService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BookmarkMergedSurfaceServiceFactory*
BookmarkMergedSurfaceServiceFactory::GetInstance() {
  static base::NoDestructor<BookmarkMergedSurfaceServiceFactory> instance;
  return instance.get();
}
// static
BrowserContextKeyedServiceFactory::TestingFactory
BookmarkMergedSurfaceServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildBookmarkMergedSurfaceService);
}

BookmarkMergedSurfaceServiceFactory::BookmarkMergedSurfaceServiceFactory()
    : ProfileKeyedServiceFactory(
          "BookmarkMergedSurfaceService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Use OTR profile for Guest session.
              // (Bookmarks can be enabled in Guest sessions under some
              // enterprise policies.)
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // No service for system profile.
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(BookmarkModelFactory::GetInstance());
  DependsOn(ManagedBookmarkServiceFactory::GetInstance());
}

BookmarkMergedSurfaceServiceFactory::~BookmarkMergedSurfaceServiceFactory() =
    default;

std::unique_ptr<KeyedService>
BookmarkMergedSurfaceServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildBookmarkMergedSurfaceService(context);
}

bool BookmarkMergedSurfaceServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}
