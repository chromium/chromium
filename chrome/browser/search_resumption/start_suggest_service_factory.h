// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_RESUMPTION_START_SUGGEST_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_RESUMPTION_START_SUGGEST_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

class StartSuggestService;

namespace search_resumption_module {
// Factory to create StarrSuggestService per regular profile. nullptr will be
// returned for incognito profile.
class StartSuggestServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static StartSuggestService* GetForBrowserContext(
      content::BrowserContext* context);
  static StartSuggestServiceFactory* GetInstance();

  StartSuggestServiceFactory(const StartSuggestServiceFactory&) = delete;
  StartSuggestServiceFactory& operator=(const StartSuggestServiceFactory&) =
      delete;

 private:
  friend class base::NoDestructor<StartSuggestServiceFactory>;

  StartSuggestServiceFactory();
  ~StartSuggestServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace search_resumption_module
#endif  // CHROME_BROWSER_SEARCH_RESUMPTION_START_SUGGEST_SERVICE_FACTORY_H_
