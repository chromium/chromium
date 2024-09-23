// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_client_factory_lacros.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/tts_client_lacros.h"

TtsClientLacros* TtsClientFactoryLacros::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<TtsClientLacros*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

TtsClientFactoryLacros* TtsClientFactoryLacros::GetInstance() {
  static base::NoDestructor<TtsClientFactoryLacros> instance;
  return instance.get();
}

TtsClientFactoryLacros::TtsClientFactoryLacros()
    : ProfileKeyedServiceFactory(
          "TtsClientLacros",
          // For incognito mode, use its original profile as browser context, so
          // that it will have the same tts support as the original profile.
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

TtsClientFactoryLacros::~TtsClientFactoryLacros() = default;

std::unique_ptr<KeyedService>
TtsClientFactoryLacros::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<TtsClientLacros>(context);
}
