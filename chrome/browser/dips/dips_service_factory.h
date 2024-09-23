// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DIPS_DIPS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class DIPSServiceImpl;

class DIPSServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static DIPSServiceFactory* GetInstance();
  static DIPSServiceImpl* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<DIPSServiceFactory>;

  DIPSServiceFactory();
  ~DIPSServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_DIPS_DIPS_SERVICE_FACTORY_H_
