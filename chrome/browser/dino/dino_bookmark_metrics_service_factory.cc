// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dino/dino_bookmark_metrics_service_factory.h"

#include <memory>

#include "base/functional/bind.h"
#include "base/no_destructor.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/dino/dino_bookmark_metrics_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace dino {

namespace {

std::unique_ptr<KeyedService> BuildDinoBookmarkMetricsService(
    content::BrowserContext* context) {
  bookmarks::BookmarkModel* model =
      BookmarkModelFactory::GetForBrowserContext(context);
  if (!model) {
    return nullptr;
  }
  return std::make_unique<DinoBookmarkMetricsService>(model);
}

}  // namespace

// static
DinoBookmarkMetricsService* DinoBookmarkMetricsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<DinoBookmarkMetricsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

// static
DinoBookmarkMetricsServiceFactory*
DinoBookmarkMetricsServiceFactory::GetInstance() {
  static base::NoDestructor<DinoBookmarkMetricsServiceFactory> instance;
  return instance.get();
}

// static
BrowserContextKeyedServiceFactory::TestingFactory
DinoBookmarkMetricsServiceFactory::GetDefaultFactory() {
  return base::BindRepeating(&BuildDinoBookmarkMetricsService);
}

DinoBookmarkMetricsServiceFactory::DinoBookmarkMetricsServiceFactory()
    : ProfileKeyedServiceFactory(
          "DinoBookmarkMetricsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithSystem(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(BookmarkModelFactory::GetInstance());
}

DinoBookmarkMetricsServiceFactory::~DinoBookmarkMetricsServiceFactory() =
    default;

std::unique_ptr<KeyedService>
DinoBookmarkMetricsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return BuildDinoBookmarkMetricsService(context);
}

bool DinoBookmarkMetricsServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

bool DinoBookmarkMetricsServiceFactory::ServiceIsNULLWhileTesting() const {
  return true;
}

}  // namespace dino
