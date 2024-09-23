// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/omnibox/autocomplete_controller_emitter_factory.h"

#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/omnibox/browser/autocomplete_controller_emitter.h"

// static
AutocompleteControllerEmitter*
AutocompleteControllerEmitterFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<AutocompleteControllerEmitter*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

// static
AutocompleteControllerEmitterFactory*
AutocompleteControllerEmitterFactory::GetInstance() {
  return base::Singleton<AutocompleteControllerEmitterFactory>::get();
}

AutocompleteControllerEmitterFactory::AutocompleteControllerEmitterFactory()
    : BrowserContextKeyedServiceFactory(
          "AutocompleteControllerEmitter",
          BrowserContextDependencyManager::GetInstance()) {}

AutocompleteControllerEmitterFactory::~AutocompleteControllerEmitterFactory() =
    default;

KeyedService* AutocompleteControllerEmitterFactory::BuildServiceInstanceFor(
    content::BrowserContext* context) const {
  return new AutocompleteControllerEmitter();
}

content::BrowserContext*
AutocompleteControllerEmitterFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  return context;
}
