// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_ISOLATED_PRERENDER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_ISOLATED_PRERENDER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;
class IsolatedPrerenderService;

class IsolatedPrerenderServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the IsolatedPrerender for |profile|.
  static IsolatedPrerenderService* GetForProfile(Profile* profile);

  static IsolatedPrerenderServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<IsolatedPrerenderServiceFactory>;

  IsolatedPrerenderServiceFactory();
  ~IsolatedPrerenderServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;

  IsolatedPrerenderServiceFactory(const IsolatedPrerenderServiceFactory&) =
      delete;
  IsolatedPrerenderServiceFactory& operator=(
      const IsolatedPrerenderServiceFactory&) = delete;
};

#endif  // CHROME_BROWSER_PREFETCH_PREFETCH_PROXY_ISOLATED_PRERENDER_SERVICE_FACTORY_H_
