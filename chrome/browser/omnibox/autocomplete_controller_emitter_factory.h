// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_OMNIBOX_AUTOCOMPLETE_CONTROLLER_EMITTER_FACTORY_H_
#define CHROME_BROWSER_OMNIBOX_AUTOCOMPLETE_CONTROLLER_EMITTER_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class AutocompleteControllerEmitter;

class AutocompleteControllerEmitterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static AutocompleteControllerEmitter* GetForBrowserContext(
      content::BrowserContext* context);

  static AutocompleteControllerEmitterFactory* GetInstance();

  AutocompleteControllerEmitterFactory(
      const AutocompleteControllerEmitterFactory&) = delete;
  AutocompleteControllerEmitterFactory& operator=(
      const AutocompleteControllerEmitterFactory&) = delete;

 private:
  friend base::DefaultSingletonTraits<AutocompleteControllerEmitterFactory>;

  AutocompleteControllerEmitterFactory();
  ~AutocompleteControllerEmitterFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_OMNIBOX_AUTOCOMPLETE_CONTROLLER_EMITTER_FACTORY_H_
