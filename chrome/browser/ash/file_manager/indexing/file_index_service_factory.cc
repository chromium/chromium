// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/indexing/file_index_service_factory.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/file_manager/indexing/file_index_service.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

namespace file_manager {
namespace {

ProfileSelections BuildFileIndexServiceProfileSelections() {
  if (!base::FeatureList::IsEnabled(ash::features::kFilesMaterializedViews)) {
    return ProfileSelections::BuildNoProfilesSelected();
  }

  return ProfileSelections::Builder()
      // Works only with regular profiles and not off the record (OTR).
      .WithRegular(ProfileSelection::kOriginalOnly)
      .WithAshInternals(ProfileSelection::kNone)
      .Build();
}

}  // namespace

// static
FileIndexService* FileIndexServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<FileIndexService*>(
      FileIndexServiceFactory::GetInstance()->GetServiceForBrowserContext(
          context, /*create=*/true));
}

// static
FileIndexServiceFactory* FileIndexServiceFactory::GetInstance() {
  static base::NoDestructor<FileIndexServiceFactory> instance;
  return instance.get();
}

FileIndexServiceFactory::FileIndexServiceFactory()
    : ProfileKeyedServiceFactory("FileIndexService",
                                 BuildFileIndexServiceProfileSelections()) {}

FileIndexServiceFactory::~FileIndexServiceFactory() = default;

std::unique_ptr<KeyedService>
FileIndexServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);

  // Do not create the service for Guest Session, as in Ash a guest session
  // uses a regular profile.
  if (profile->IsGuestSession()) {
    return nullptr;
  }

  return std::make_unique<FileIndexService>(profile);
}

bool FileIndexServiceFactory::ServiceIsCreatedWithBrowserContext() const {
  // If true, it initializes the storage at log in, so that the worker can watch
  // files in the background.
  return true;
}

}  // namespace file_manager
