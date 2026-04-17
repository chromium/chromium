// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/accessibility/live_caption/live_translate_controller_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "chrome/browser/on_device_translation/service_controller_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/live_caption/features.h"
#include "components/live_caption/google_api_translation_dispatcher.h"
#include "components/live_caption/live_translate_controller.h"
#include "components/live_caption/translation_dispatcher_on_device.h"
#include "components/on_device_translation/buildflags/buildflags.h"
#include "components/on_device_translation/service/service_launcher.h"
#include "components/on_device_translation/service_controller.h"
#include "components/on_device_translation/service_controller_manager.h"
#include "google_apis/google_api_keys.h"

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
    content::BrowserContext* context) const {
  Profile* profile = Profile::FromBrowserContext(context);
  std::unique_ptr<TranslationDispatcher> on_device_dispatcher;
  std::unique_ptr<TranslationDispatcher> google_api_dispatcher;

  // Only set on_device_dispatcher if feature flag is set.
  if (base::FeatureList::IsEnabled(
          live_caption::kLiveCaptionOnDeviceTranslation)) {
    on_device_dispatcher = std::make_unique<TranslationDispatcherOnDevice>(
        std::make_unique<
            on_device_translation::OnDeviceTranslationServiceController>(
            on_device_translation::CreateOnDeviceTranslationServiceLauncher(),
            /*service_display_name_suffix=*/""));
  }
  google_api_dispatcher = std::make_unique<GoogleApiTranslationDispatcher>(
      google_apis::GetAPIKey(), context);

  return std::make_unique<LiveTranslateController>(
      profile->GetPrefs(), std::move(on_device_dispatcher),
      std::move(google_api_dispatcher));
}

}  // namespace captions
