// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service_factory.h"

#include "chrome/browser/picture_in_picture/hats/auto_picture_in_picture_hats_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/hats/hats_service.h"
#include "chrome/browser/ui/hats/hats_service_factory.h"
#include "media/base/media_switches.h"

// static
AutoPictureInPictureHatsServiceFactory*
AutoPictureInPictureHatsServiceFactory::GetInstance() {
  static base::NoDestructor<AutoPictureInPictureHatsServiceFactory> instance;
  return instance.get();
}

// static
AutoPictureInPictureHatsService*
AutoPictureInPictureHatsServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<AutoPictureInPictureHatsService*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/true));
}

AutoPictureInPictureHatsServiceFactory::AutoPictureInPictureHatsServiceFactory()
    : ProfileKeyedServiceFactory(
          "AutoPictureInPictureHatsService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(HatsServiceFactory::GetInstance());
}

std::unique_ptr<KeyedService>
AutoPictureInPictureHatsServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(media::kAutoPictureInPictureSurveys)) {
    return nullptr;
  }

  if (context->IsOffTheRecord()) {
    return nullptr;
  }

  Profile* profile = Profile::FromBrowserContext(context);
  auto* hats_service =
      HatsServiceFactory::GetForProfile(profile, /*create_if_necessary=*/true);

  // If there is no HaTS service, or the HaTS service reports the user is not
  // eligible to be surveyed by HaTS, do not create the service.
  if (!hats_service ||
      !hats_service->CanShowAnySurvey(/*user_prompted=*/false)) {
    return nullptr;
  }

  return std::make_unique<AutoPictureInPictureHatsService>(profile);
}
