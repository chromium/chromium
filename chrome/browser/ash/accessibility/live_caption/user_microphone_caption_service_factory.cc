// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/live_caption/user_microphone_caption_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

// static
SystemLiveCaptionService* UserMicrophoneCaptionServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SystemLiveCaptionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
UserMicrophoneCaptionServiceFactory*
UserMicrophoneCaptionServiceFactory::GetInstance() {
  static base::NoDestructor<UserMicrophoneCaptionServiceFactory> factory;
  return factory.get();
}

UserMicrophoneCaptionServiceFactory::UserMicrophoneCaptionServiceFactory()
    : ProfileKeyedServiceFactory(
          "SystemLiveCaptionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .WithGuest(ProfileSelection::kOriginalOnly)
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(::captions::LiveCaptionControllerFactory::GetInstance());
}

UserMicrophoneCaptionServiceFactory::~UserMicrophoneCaptionServiceFactory() =
    default;

std::unique_ptr<KeyedService>
UserMicrophoneCaptionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SystemLiveCaptionService>(
      Profile::FromBrowserContext(context),
      SystemLiveCaptionService::AudioSource::kUserMicrophone);
}

}  // namespace ash
