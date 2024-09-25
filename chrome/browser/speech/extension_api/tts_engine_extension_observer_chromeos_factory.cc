// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos_factory.h"

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "chrome/browser/speech/extension_api/tts_engine_extension_observer_chromeos.h"
#include "extensions/browser/event_router_factory.h"

// static
TtsEngineExtensionObserverChromeOS*
TtsEngineExtensionObserverChromeOSFactory::GetForProfile(Profile* profile) {
  return static_cast<TtsEngineExtensionObserverChromeOS*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
TtsEngineExtensionObserverChromeOSFactory*
TtsEngineExtensionObserverChromeOSFactory::GetInstance() {
  return base::Singleton<TtsEngineExtensionObserverChromeOSFactory>::get();
}

TtsEngineExtensionObserverChromeOSFactory::
    TtsEngineExtensionObserverChromeOSFactory()
    : ProfileKeyedServiceFactory(
          "TtsEngineExtensionObserverChromeOS",
          // If given an incognito profile (including the Chrome OS login
          // profile), share the service with the original profile.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kRedirectedToOriginal)
              .Build()) {
  DependsOn(extensions::EventRouterFactory::GetInstance());
}

TtsEngineExtensionObserverChromeOSFactory::
    ~TtsEngineExtensionObserverChromeOSFactory() = default;

KeyedService*
TtsEngineExtensionObserverChromeOSFactory::BuildServiceInstanceFor(
    content::BrowserContext* profile) const {
  return new TtsEngineExtensionObserverChromeOS(static_cast<Profile*>(profile));
}
