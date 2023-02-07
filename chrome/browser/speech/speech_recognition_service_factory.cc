// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/chrome_speech_recognition_service.h"
#include "chrome/browser/speech/speech_recognition_service.h"

// static
speech::SpeechRecognitionService*
SpeechRecognitionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<speech::SpeechRecognitionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SpeechRecognitionServiceFactory*
SpeechRecognitionServiceFactory::GetInstance() {
  static base::NoDestructor<SpeechRecognitionServiceFactory> instance;
  return instance.get();
}

SpeechRecognitionServiceFactory::SpeechRecognitionServiceFactory()
    : ProfileKeyedServiceFactory(
          "SpeechRecognitionService",
          // Incognito profiles should use their own instance of the browser
          // context.
          ProfileSelections::BuildForRegularAndIncognito()) {}

SpeechRecognitionServiceFactory::~SpeechRecognitionServiceFactory() = default;

KeyedService* SpeechRecognitionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new speech::ChromeSpeechRecognitionService(context);
}

// static
void SpeechRecognitionServiceFactory::EnsureFactoryBuilt() {
  SpeechRecognitionServiceFactory::GetInstance();
}
