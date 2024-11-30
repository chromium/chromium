// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager_factory.h"

#include <utility>

#include "base/feature_list.h"
#include "base/memory/ptr_util.h"
#include "base/types/pass_key.h"
#include "chrome/browser/media/android/cdm/media_drm_origin_id_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "media/base/media_switches.h"

// static
MediaDrmOriginIdManager* MediaDrmOriginIdManagerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MediaDrmOriginIdManager*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MediaDrmOriginIdManagerFactory* MediaDrmOriginIdManagerFactory::GetInstance() {
  static base::NoDestructor<MediaDrmOriginIdManagerFactory> instance;
  return instance.get();
}

MediaDrmOriginIdManagerFactory::MediaDrmOriginIdManagerFactory()
    // No service for Incognito mode.
    : ProfileKeyedServiceFactory(
          "MediaDrmOriginIdManager",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              .Build()) {}

MediaDrmOriginIdManagerFactory::~MediaDrmOriginIdManagerFactory() = default;

std::unique_ptr<KeyedService>
MediaDrmOriginIdManagerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  return std::make_unique<MediaDrmOriginIdManager>(
      profile->GetPrefs(), base::PassKey<MediaDrmOriginIdManagerFactory>());
}

bool MediaDrmOriginIdManagerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  // Create this service when the context is created if the feature is enabled.
  // Creation will end up calling GetBrowserContextToUse() above which returns
  // NULL for incognito contexts, and thus no instance will be created for them.
  return base::FeatureList::IsEnabled(media::kMediaDrmPreprovisioning);
}
