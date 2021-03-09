// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NSS_SERVICE_CHROMEOS_FACTORY_H_
#define CHROME_BROWSER_NET_NSS_SERVICE_CHROMEOS_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class NssServiceChromeOS;

namespace content {
class BrowserContext;
}

class NssServiceChromeOSFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the NssServiceChromeOS for |browser_context|.
  static NssServiceChromeOS* GetForContext(
      content::BrowserContext* browser_context);

 private:
  friend base::NoDestructor<NssServiceChromeOSFactory>;

  NssServiceChromeOSFactory();
  NssServiceChromeOSFactory(const NssServiceChromeOSFactory&) = delete;
  NssServiceChromeOSFactory& operator=(const NssServiceChromeOSFactory&) =
      delete;
  ~NssServiceChromeOSFactory() override;

  static NssServiceChromeOSFactory& GetInstance();

  // BrowserContextKeyedServiceFactory implementation:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_NET_NSS_SERVICE_CHROMEOS_FACTORY_H_
