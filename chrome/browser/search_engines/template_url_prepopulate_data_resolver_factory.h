// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_FACTORY_H_
#define CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace TemplateURLPrepopulateData {

class Resolver;

// Profile-keyed service factory for `TemplateURLPrepopulateData::Resolver`.
class ResolverFactory : public ProfileKeyedServiceFactory {
 public:
  static Resolver* GetForProfile(Profile* profile);
  static ResolverFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<ResolverFactory>;

  ResolverFactory();
  ~ResolverFactory() override;

  // BrowserContextKeyedServiceFactory
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace TemplateURLPrepopulateData

#endif  // CHROME_BROWSER_SEARCH_ENGINES_TEMPLATE_URL_PREPOPULATE_DATA_RESOLVER_FACTORY_H_
