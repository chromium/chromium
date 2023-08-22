// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_caption_controller_factory.h"

#include "base/no_destructor.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/live_caption_controller.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/profiles/profile_helper.h"
#endif

namespace captions {

// static
LiveCaptionController* LiveCaptionControllerFactory::GetForProfile(
    Profile* profile) {
  return static_cast<LiveCaptionController*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
LiveCaptionController* LiveCaptionControllerFactory::GetForProfileIfExists(
    Profile* profile) {
  return static_cast<LiveCaptionController*>(
      GetInstance()->GetServiceForBrowserContext(profile, /*create=*/false));
}

// static
LiveCaptionControllerFactory* LiveCaptionControllerFactory::GetInstance() {
  static base::NoDestructor<LiveCaptionControllerFactory> factory;
  return factory.get();
}

LiveCaptionControllerFactory::LiveCaptionControllerFactory()
    : ProfileKeyedServiceFactory(
          "LiveCaptionController",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // Use OTR profile for Guest Session.
              .WithGuest(ProfileSelection::kOffTheRecordOnly)
              // No service for system profile.
              .WithSystem(ProfileSelection::kNone)
              // ChromeOS creates various profiles (login, lock screen...) that
              // do not need the Live Caption controller.
              .WithAshInternals(ProfileSelection::kNone)
              .Build()) {}

LiveCaptionControllerFactory::~LiveCaptionControllerFactory() = default;

bool LiveCaptionControllerFactory::ServiceIsCreatedWithBrowserContext() const {
  return true;
}

std::unique_ptr<KeyedService>
LiveCaptionControllerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<LiveCaptionController>(
      Profile::FromBrowserContext(context)->GetPrefs(),
      g_browser_process->local_state(),
      g_browser_process->GetApplicationLocale(), context);
}

}  // namespace captions
