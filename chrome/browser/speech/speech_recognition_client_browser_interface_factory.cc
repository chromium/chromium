// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/speech_recognition_client_browser_interface_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/speech_recognition_client_browser_interface.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

// static
speech::SpeechRecognitionClientBrowserInterface*
SpeechRecognitionClientBrowserInterfaceFactory::GetForProfile(
    Profile* profile) {
  return static_cast<speech::SpeechRecognitionClientBrowserInterface*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

// static
SpeechRecognitionClientBrowserInterfaceFactory*
SpeechRecognitionClientBrowserInterfaceFactory::GetInstance() {
  static base::NoDestructor<SpeechRecognitionClientBrowserInterfaceFactory>
      instance;
  return instance.get();
}

SpeechRecognitionClientBrowserInterfaceFactory::
    SpeechRecognitionClientBrowserInterfaceFactory()
    : BrowserContextKeyedServiceFactory(
          "SpeechRecognitionClientBrowserInterface",
          BrowserContextDependencyManager::GetInstance()) {}

SpeechRecognitionClientBrowserInterfaceFactory::
    ~SpeechRecognitionClientBrowserInterfaceFactory() = default;

KeyedService*
SpeechRecognitionClientBrowserInterfaceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new speech::SpeechRecognitionClientBrowserInterface(context);
}

// Incognito profiles should use their own instance of the browser context.
content::BrowserContext*
SpeechRecognitionClientBrowserInterfaceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return chrome::GetBrowserContextOwnInstanceInIncognito(context);
}
