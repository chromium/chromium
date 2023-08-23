// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_LANGUAGE_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace language {
class AcceptLanguagesService;
}

// AcceptLanguagesServiceFactory is a way to associate an
// AcceptLanguagesService instance to a BrowserContext.
class AcceptLanguagesServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static language::AcceptLanguagesService* GetForBrowserContext(
      content::BrowserContext* browser_context);
  static AcceptLanguagesServiceFactory* GetInstance();

  AcceptLanguagesServiceFactory(const AcceptLanguagesServiceFactory&) = delete;
  AcceptLanguagesServiceFactory& operator=(
      const AcceptLanguagesServiceFactory&) = delete;

 private:
  friend base::NoDestructor<AcceptLanguagesServiceFactory>;

  AcceptLanguagesServiceFactory();
  ~AcceptLanguagesServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_LANGUAGE_ACCEPT_LANGUAGES_SERVICE_FACTORY_H_
