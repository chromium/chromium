// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_LOADER_FACTORY_H_
#define CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_LOADER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class CroshLoader;
class Profile;

class CroshLoaderFactory : public BrowserContextKeyedServiceFactory {
 public:
  static CroshLoader* GetForProfile(Profile* profile);
  static CroshLoaderFactory* GetInstance();

 private:
  friend class base::NoDestructor<CroshLoaderFactory>;

  CroshLoaderFactory();
  ~CroshLoaderFactory() override;
  CroshLoaderFactory(const CroshLoaderFactory&) = delete;
  CroshLoaderFactory& operator=(const CroshLoaderFactory&) = delete;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_ASH_WEB_APPLICATIONS_CROSH_LOADER_FACTORY_H_
