// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_FACTORY_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <class T>
class NoDestructor;
}  // namespace base

namespace speech {
class SpeechRecognitionClientBrowserInterface;
}  // namespace speech

// Factory to get or create an instance of
// SpeechRecognitionClientBrowserInterface from a Profile.
class SpeechRecognitionClientBrowserInterfaceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static speech::SpeechRecognitionClientBrowserInterface* GetForProfile(
      Profile* profile);

 private:
  friend class base::NoDestructor<
      SpeechRecognitionClientBrowserInterfaceFactory>;
  static SpeechRecognitionClientBrowserInterfaceFactory* GetInstance();

  SpeechRecognitionClientBrowserInterfaceFactory();
  ~SpeechRecognitionClientBrowserInterfaceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_FACTORY_H_
