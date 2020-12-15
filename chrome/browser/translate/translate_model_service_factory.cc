// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_model_service_factory.h"

#include "chrome/browser/profiles/incognito_helpers.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/translate/translate_model_service_impl.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/translate/core/common/translate_util.h"
#include "content/public/browser/browser_context.h"

// static
TranslateModelServiceImpl* TranslateModelServiceFactory::GetForProfile(
    Profile* profile) {
  if (translate::IsTFLiteLanguageDetectionEnabled()) {
    return static_cast<TranslateModelServiceImpl*>(
        GetInstance()->GetServiceForBrowserContext(profile, true));
  }
  return nullptr;
}

// static
TranslateModelServiceFactory* TranslateModelServiceFactory::GetInstance() {
  static base::NoDestructor<TranslateModelServiceFactory> factory;
  return factory.get();
}

TranslateModelServiceFactory::TranslateModelServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "TranslateModelService",
          BrowserContextDependencyManager::GetInstance()) {}

TranslateModelServiceFactory::~TranslateModelServiceFactory() = default;

KeyedService* TranslateModelServiceFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new TranslateModelServiceImpl();
}

content::BrowserContext* TranslateModelServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  // Use the original profile's TranslateModelService, even in Incognito mode.
  return chrome::GetBrowserContextRedirectedInIncognito(context);
}
