// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/cros_speech_recognition_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/cros_speech_recognition_service.h"
#include "chrome/browser/speech/speech_recognition_service.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
speech::SpeechRecognitionService*
CrosSpeechRecognitionServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<speech::SpeechRecognitionService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
CrosSpeechRecognitionServiceFactory*
CrosSpeechRecognitionServiceFactory::GetInstance() {
  static base::NoDestructor<CrosSpeechRecognitionServiceFactory> instance;
  return instance.get();
}

CrosSpeechRecognitionServiceFactory::CrosSpeechRecognitionServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "SpeechRecognitionService",
          BrowserContextDependencyManager::GetInstance()) {}

CrosSpeechRecognitionServiceFactory::~CrosSpeechRecognitionServiceFactory() =
    default;

KeyedService* CrosSpeechRecognitionServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new speech::CrosSpeechRecognitionService(context);
}

// Incognito profiles should use their own instance of the browser context.
content::BrowserContext*
CrosSpeechRecognitionServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
