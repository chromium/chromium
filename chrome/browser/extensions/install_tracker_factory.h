// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace extensions {

class InstallTracker;

class InstallTrackerFactory : public ProfileKeyedServiceFactory {
 public:
  InstallTrackerFactory(const InstallTrackerFactory&) = delete;
  InstallTrackerFactory& operator=(const InstallTrackerFactory&) = delete;

  static InstallTracker* GetForBrowserContext(content::BrowserContext* context);
  static InstallTrackerFactory* GetInstance();

 private:
  friend base::NoDestructor<InstallTrackerFactory>;

  InstallTrackerFactory();
  ~InstallTrackerFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_INSTALL_TRACKER_FACTORY_H_
