// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service.h"
#include "chrome/browser/speech/speech_recognition_service.h"

// static
speech::SpeechRecognitionService*
CrosSpeechRecognitionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<speech::SpeechRecognitionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrosSpeechRecognitionServiceFactory*
CrosSpeechRecognitionServiceFactory::GetInstanceForTest() {
  return GetInstance();
}

// static
CrosSpeechRecognitionServiceFactory*
CrosSpeechRecognitionServiceFactory::GetInstance() {
  static base::NoDestructor<CrosSpeechRecognitionServiceFactory> instance;
  return instance.get();
}

CrosSpeechRecognitionServiceFactory::CrosSpeechRecognitionServiceFactory()
    : ProfileKeyedServiceFactory(
          "SpeechRecognitionService",
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
              .Build()) {}

CrosSpeechRecognitionServiceFactory::~CrosSpeechRecognitionServiceFactory() =
    default;

std::unique_ptr<KeyedService>
CrosSpeechRecognitionServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<speech::CrosSpeechRecognitionService>(context);
}

// static
void CrosSpeechRecognitionServiceFactory::EnsureFactoryBuilt() {
  GetInstance();
}
