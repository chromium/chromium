// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
#define CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace translate {
class TranslateAcceptLanguages;
}

// TranslateAcceptLanguagesFactory is a way to associate a
// TranslateAcceptLanguages instance to a BrowserContext.
class TranslateAcceptLanguagesFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static translate::TranslateAcceptLanguages* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static TranslateAcceptLanguagesFactory* GetInstance();

  TranslateAcceptLanguagesFactory(const TranslateAcceptLanguagesFactory&) =
      delete;
  TranslateAcceptLanguagesFactory& operator=(
      const TranslateAcceptLanguagesFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<TranslateAcceptLanguagesFactory>;

  TranslateAcceptLanguagesFactory();
  ~TranslateAcceptLanguagesFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_TRANSLATE_TRANSLATE_ACCEPT_LANGUAGES_FACTORY_H_
