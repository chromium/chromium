// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T> struct DefaultSingletonTraits;
}

namespace extensions {

class InstallTracker;

class InstallTrackerFactory : public BrowserContextKeyedServiceFactory {
 public:
  InstallTrackerFactory(const InstallTrackerFactory&) = delete;
  InstallTrackerFactory& operator=(const InstallTrackerFactory&) = delete;

  static InstallTracker* GetForBrowserContext(content::BrowserContext* context);
  static InstallTrackerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<InstallTrackerFactory>;

  InstallTrackerFactory();
  ~InstallTrackerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_FACTORY_H_
