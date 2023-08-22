// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LANGUAGE_URL_LANGUAGE_HISTOGRAM_FACTORY_H_
#define CHROME_BROWSER_LANGUAGE_URL_LANGUAGE_HISTOGRAM_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace language {
class UrlLanguageHistogram;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

class UrlLanguageHistogramFactory : public ProfileKeyedServiceFactory {
 public:
  static UrlLanguageHistogramFactory* GetInstance();
  static language::UrlLanguageHistogram* GetForBrowserContext(
      content::BrowserContext* browser_context);

  UrlLanguageHistogramFactory(const UrlLanguageHistogramFactory&) = delete;
  UrlLanguageHistogramFactory& operator=(const UrlLanguageHistogramFactory&) =
      delete;

 private:
  friend base::NoDestructor<UrlLanguageHistogramFactory>;

  UrlLanguageHistogramFactory();
  ~UrlLanguageHistogramFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

#endif  // CHROME_BROWSER_LANGUAGE_URL_LANGUAGE_HISTOGRAM_FACTORY_H_
