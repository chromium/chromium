// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DIPS_DIPS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DIPS_DIPS_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class DIPSService;

class DIPSServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static DIPSServiceFactory* GetInstance();
  static DIPSService* GetForBrowserContext(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<DIPSServiceFactory>;

  DIPSServiceFactory();
  ~DIPSServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_DIPS_DIPS_SERVICE_FACTORY_H_
