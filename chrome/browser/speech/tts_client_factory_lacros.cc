// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/speech/tts_client_factory_lacros.h"

#include "base/no_destructor.h"
#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/speech/tts_client_lacros.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"

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
    : BrowserContextKeyedServiceFactory(
          "TtsClientLacros",
          BrowserContextDependencyManager::GetInstance()) {}

TtsClientFactoryLacros::~TtsClientFactoryLacros() = default;

content::BrowserContext* TtsClientFactoryLacros::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // For incognito mode, use its original profile as browser context, so that
  // it will have the same tts support as the original profile.
  bool is_off_record = Profile::FromBrowserContext(context)->IsOffTheRecord();
  content::BrowserContext* context_to_use =
      is_off_record ? chrome::GetBrowserContextRedirectedInIncognito(context)
                    : context;
  return context_to_use;
}

KeyedService* TtsClientFactoryLacros::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new TtsClientLacros(context);
}
