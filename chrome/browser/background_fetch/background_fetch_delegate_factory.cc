// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/background_fetch/background_fetch_delegate_factory.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/background_fetch/background_fetch_delegate_impl.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/download/background_download_service_factory.h"
#include "chrome/browser/metrics/ukm_background_recorder_service.h"
#include "chrome/browser/offline_items_collection/offline_content_aggregator_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "content/public/browser/background_fetch_delegate.h"

// static
BackgroundFetchDelegateImpl* BackgroundFetchDelegateFactory::GetForProfile(
    Profile* profile) {
  return static_cast<BackgroundFetchDelegateImpl*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
BackgroundFetchDelegateFactory* BackgroundFetchDelegateFactory::GetInstance() {
  return base::Singleton<BackgroundFetchDelegateFactory>::get();
}

BackgroundFetchDelegateFactory::BackgroundFetchDelegateFactory()
    : ProfileKeyedServiceFactory(
          "BackgroundFetchService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/1418376): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(BackgroundDownloadServiceFactory::GetInstance());
  DependsOn(HostContentSettingsMapFactory::GetInstance());
  DependsOn(OfflineContentAggregatorFactory::GetInstance());
  DependsOn(ukm::UkmBackgroundRecorderFactory::GetInstance());
}

BackgroundFetchDelegateFactory::~BackgroundFetchDelegateFactory() {}

KeyedService* BackgroundFetchDelegateFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new BackgroundFetchDelegateImpl(Profile::FromBrowserContext(context));
}
