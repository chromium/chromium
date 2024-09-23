// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"
#include "chrome/browser/ash/accessibility/live_caption/system_live_caption_service.h"
#include "chrome/browser/profiles/profile.h"

namespace ash {

// static
SystemLiveCaptionService* SystemLiveCaptionServiceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<SystemLiveCaptionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SystemLiveCaptionServiceFactory*
SystemLiveCaptionServiceFactory::GetInstance() {
  static base::NoDestructor<SystemLiveCaptionServiceFactory> factory;
  return factory.get();
}

SystemLiveCaptionServiceFactory::SystemLiveCaptionServiceFactory()
    : ProfileKeyedServiceFactory(
          "SystemLiveCaptionService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {
  DependsOn(::captions::LiveCaptionControllerFactory::GetInstance());
}

SystemLiveCaptionServiceFactory::~SystemLiveCaptionServiceFactory() = default;

std::unique_ptr<KeyedService>
SystemLiveCaptionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<SystemLiveCaptionService>(
      Profile::FromBrowserContext(context));
}

}  // namespace ash
