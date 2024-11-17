// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class SpeechRecognitionPrivateManager;

// Factory to get or create an instance of SpeechRecognitionPrivateManager from
// a browser context.
class SpeechRecognitionPrivateManagerFactory
    : public ProfileKeyedServiceFactory {
 public:
  // Gets or creates an instance of SpeechRecognitionPrivateManager from a
  // browser context.
  static SpeechRecognitionPrivateManager* GetForBrowserContext(
      content::BrowserContext* context);

  // Retrieves the singleton instance of the factory.
  static SpeechRecognitionPrivateManagerFactory* GetInstance();

  SpeechRecognitionPrivateManagerFactory(
      const SpeechRecognitionPrivateManagerFactory&) = delete;
  SpeechRecognitionPrivateManagerFactory& operator=(
      const SpeechRecognitionPrivateManagerFactory&) = delete;

 private:
  friend class base::NoDestructor<SpeechRecognitionPrivateManagerFactory>;

  SpeechRecognitionPrivateManagerFactory();
  ~SpeechRecognitionPrivateManagerFactory() override = default;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_SPEECH_SPEECH_RECOGNITION_PRIVATE_MANAGER_FACTORY_H_
