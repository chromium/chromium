// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace base {
template <class T>
class NoDestructor;
}  // namespace base

namespace speech {
class SpeechRecognitionService;
}  // namespace speech

// Factory to get or create an instance of SpeechRecognitionServiceFactory from
// a Profile.
class SpeechRecognitionServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static speech::SpeechRecognitionService* GetForProfile(Profile* profile);

 private:
  friend class base::NoDestructor<SpeechRecognitionServiceFactory>;
  static SpeechRecognitionServiceFactory* GetInstance();

  SpeechRecognitionServiceFactory();
  ~SpeechRecognitionServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SPEECH_SPEECH_RECOGNITION_SERVICE_FACTORY_H_
