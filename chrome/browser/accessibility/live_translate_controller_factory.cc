// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_translate_controller_factory.h"

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/live_translate_controller.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

namespace captions {

// static
LiveTranslateController* LiveTranslateControllerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<LiveTranslateController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LiveTranslateControllerFactory* LiveTranslateControllerFactory::GetInstance() {
  static base::NoDestructor<LiveTranslateControllerFactory> factory;
  return factory.get();
}

LiveTranslateControllerFactory::LiveTranslateControllerFactory()
    : ProfileKeyedServiceFactory(
          "LiveTranslateController",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Use OTR profile for Guest Session.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // No service for system profile.
              .WithSystem(ProfileSelection::kNone)
              // ChromeOS creates various profiles (login, lock screen...) that
              // do not need the Live Translate controller.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

LiveTranslateControllerFactory::~LiveTranslateControllerFactory() = default;

bool LiveTranslateControllerFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

std::unique_ptr<KeyedService>
LiveTranslateControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* browser_context) const {
  return std::make_unique<LiveTranslateController>(
      Profile::FromBrowserContext(browser_context)->GetPrefs(),
      browser_context);
}

}  // namespace captions
