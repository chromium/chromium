// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_FACTORY_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace speech {
class SpeechRecognitionClientBrowserInterface;
}  // namespace speech

// Factory to get or create an instance of
// SpeechRecognitionClientBrowserInterface from a Profile.
class SpeechRecognitionClientBrowserInterfaceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static speech::SpeechRecognitionClientBrowserInterface* GetForProfile(
      Profile* profile);

  static void EnsureFactoryBuilt();

 private:
  friend class base::NoDestructor<
      SpeechRecognitionClientBrowserInterfaceFactory>;
  static SpeechRecognitionClientBrowserInterfaceFactory* GetInstance();

  SpeechRecognitionClientBrowserInterfaceFactory();
  ~SpeechRecognitionClientBrowserInterfaceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_CLIENT_BROWSER_INTERFACE_FACTORY_H_
