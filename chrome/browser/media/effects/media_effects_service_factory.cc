// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/media/effects/media_effects_service_factory.h"
#include "chrome/browser/profiles/profile.h"

// static
MediaEffectsService* MediaEffectsServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<MediaEffectsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
MediaEffectsServiceFactory* MediaEffectsServiceFactory::GetInstance() {
  static base::NoDestructor<MediaEffectsServiceFactory> instance;
  return instance.get();
}

MediaEffectsServiceFactory::MediaEffectsServiceFactory()
    : ProfileKeyedServiceFactory(
          "MediaEffectsServiceFactory",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              .WithGuest(ProfileSelection::kOwnInstance)
              .Build()) {}

MediaEffectsServiceFactory::~MediaEffectsServiceFactory() = default;

std::unique_ptr<KeyedService>
MediaEffectsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<MediaEffectsService>(
      Profile::FromBrowserContext(context));
}
