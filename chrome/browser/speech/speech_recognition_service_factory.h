// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace speech {
class SpeechRecognitionService;
}  // namespace speech

// Factory to get or create an instance of SpeechRecognitionServiceFactory from
// a Profile.
class SpeechRecognitionServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static speech::SpeechRecognitionService* GetForProfile(Profile* profile);

  static void EnsureFactoryBuilt();

 private:
  friend class base::NoDestructor<SpeechRecognitionServiceFactory>;
  static SpeechRecognitionServiceFactory* GetInstance();

  SpeechRecognitionServiceFactory();
  ~SpeechRecognitionServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_FACTORY_H_
