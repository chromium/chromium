// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/dlp/dlp_download_observer_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/chromeos/policy/dlp/dlp_download_observer.h"
#include "chrome/browser/download/download_core_service.h"
#include "chrome/browser/download/download_core_service_factory.h"
#include "chrome/browser/download/download_offline_content_provider.h"
#include "chrome/browser/download/simple_download_manager_coordinator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_key.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/profiles/profile_selections.h"

namespace policy {

DlpDownloadObserverFactory::DlpDownloadObserverFactory()
    : ProfileKeyedServiceFactory(
          "DlpDownloadObserver",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kNone)
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {
  DependsOn(SimpleDownloadManagerCoordinatorFactory::GetInstance());
}

// static
DlpDownloadObserverFactory* DlpDownloadObserverFactory::GetInstance() {
  static base::NoDestructor<DlpDownloadObserverFactory> instance;
  return &*instance;
}

bool DlpDownloadObserverFactory::ServiceIsCreatedWithBrowserContext() const {
  // Build to attach to download manager
  return true;
}

std::unique_ptr<KeyedService>
DlpDownloadObserverFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  if (profile->IsGuestSession()) {
    return nullptr;
  }
  return std::make_unique<DlpDownloadObserver>(profile->GetProfileKey());
}

}  // namespace policy
