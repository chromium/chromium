// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SPEECH_TTS_CLIENT_FACTORY_LACROS_H_
#define CHROME_BROWSER_SPEECH_TTS_CLIENT_FACTORY_LACROS_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class TtsClientLacros;

// Service factory to create TtsClientLacros per BrowserContext.
// Note that OTR browser context uses its original profile's browser context,
// and won't create a separate TtsClientLacros.
class TtsClientFactoryLacros : public ProfileKeyedServiceFactory {
 public:
  // Returns the TtsClientLacros for |browser_context|, creating it if
  // it is not yet created.
  static TtsClientLacros* GetForBrowserContext(
      content::BrowserContext* context);

  // Returns the TtsClientFactoryLacros instance.
  static TtsClientFactoryLacros* GetInstance();

 private:
  friend class base::NoDestructor<TtsClientFactoryLacros>;
  TtsClientFactoryLacros();
  ~TtsClientFactoryLacros() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_SPEECH_TTS_CLIENT_FACTORY_LACROS_H_
