// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace browser_switcher {

class BrowserSwitcherService;

// Creates a |BrowserSwitcherService| for a BrowserContext.
class BrowserSwitcherServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static BrowserSwitcherServiceFactory* GetInstance();
  static BrowserSwitcherService* GetForBrowserContext(
      content::BrowserContext* browser_context);

  BrowserSwitcherServiceFactory(const BrowserSwitcherServiceFactory&) = delete;
  BrowserSwitcherServiceFactory& operator=(
      const BrowserSwitcherServiceFactory&) = delete;

 private:
  friend struct base::DefaultSingletonTraits<BrowserSwitcherServiceFactory>;

  BrowserSwitcherServiceFactory();
  ~BrowserSwitcherServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_FACTORY_H_
