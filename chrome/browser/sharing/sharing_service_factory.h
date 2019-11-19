// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SHARING_SHARING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SHARING_SHARING_SERVICE_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace content {
class BrowserContext;
}

class SharingService;

// Factory for SharingService.
class SharingServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns singleton instance of SharingServiceFactory.
  static SharingServiceFactory* GetInstance();

  // Returns the SharingService associated with |context|.
  static SharingService* GetForBrowserContext(content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<SharingServiceFactory>;

  SharingServiceFactory();
  ~SharingServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(SharingServiceFactory);
};

#endif  // CHROME_BROWSER_SHARING_SHARING_SERVICE_FACTORY_H_
