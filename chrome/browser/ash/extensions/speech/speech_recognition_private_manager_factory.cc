// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/extensions/speech/speech_recognition_private_manager_factory.h"

#include "chrome/browser/ash/extensions/speech/speech_recognition_private_manager.h"
#include "chrome/browser/profiles/profile_selections.h"
#include "extensions/browser/event_router_factory.h"

namespace extensions {

// static
SpeechRecognitionPrivateManager*
SpeechRecognitionPrivateManagerFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<SpeechRecognitionPrivateManager*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
SpeechRecognitionPrivateManagerFactory*
SpeechRecognitionPrivateManagerFactory::GetInstance() {
  static base::NoDestructor<SpeechRecognitionPrivateManagerFactory> instance;
  return instance.get();
}

SpeechRecognitionPrivateManagerFactory::SpeechRecognitionPrivateManagerFactory()
    : ProfileKeyedServiceFactory(
          "SpeechRecognitionApiManager",
          // Incognito profiles should use their own instance of the browser
          // context.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOwnInstance)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOwnInstance)
              .Build()) {
  DependsOn(EventRouterFactory::GetInstance());
}

KeyedService* SpeechRecognitionPrivateManagerFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new SpeechRecognitionPrivateManager(context);
}

}  // namespace extensions
