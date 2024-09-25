// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_CHROMEOS_FACTORY_H_
#define CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_CHROMEOS_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class TtsEngineExtensionObserverChromeOS;

// Factory to load one instance of TtsExtensionLoaderChromeOs per profile.
class TtsEngineExtensionObserverChromeOSFactory
    : public ProfileKeyedServiceFactory {
 public:
  static TtsEngineExtensionObserverChromeOS* GetForProfile(Profile* profile);

  static TtsEngineExtensionObserverChromeOSFactory* GetInstance();

  TtsEngineExtensionObserverChromeOSFactory(
      const TtsEngineExtensionObserverChromeOSFactory&) = delete;
  TtsEngineExtensionObserverChromeOSFactory& operator=(
      const TtsEngineExtensionObserverChromeOSFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<
      TtsEngineExtensionObserverChromeOSFactory>;

  TtsEngineExtensionObserverChromeOSFactory();
  ~TtsEngineExtensionObserverChromeOSFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SPEECH_EXTENSION_API_TTS_ENGINE_EXTENSION_OBSERVER_CHROMEOS_FACTORY_H_
