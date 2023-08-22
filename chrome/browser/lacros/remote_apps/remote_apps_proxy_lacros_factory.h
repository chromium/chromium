// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_REMOTE_APPS_REMOTE_APPS_PROXY_LACROS_FACTORY_H_
#define CHROME_BROWSER_LACROS_REMOTE_APPS_REMOTE_APPS_PROXY_LACROS_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace chromeos {

class RemoteAppsProxyLacros;

// Factory for the `RemoteAppsProxyLacros` KeyedService.
class RemoteAppsProxyLacrosFactory : public ProfileKeyedServiceFactory {
 public:
  static RemoteAppsProxyLacros* GetForBrowserContext(
      content::BrowserContext* browser_context);

  static RemoteAppsProxyLacrosFactory* GetInstance();

  RemoteAppsProxyLacrosFactory(const RemoteAppsProxyLacrosFactory&) = delete;
  RemoteAppsProxyLacrosFactory& operator=(const RemoteAppsProxyLacrosFactory&) =
      delete;

 private:
  friend class base::NoDestructor<RemoteAppsProxyLacrosFactory>;

  RemoteAppsProxyLacrosFactory();
  ~RemoteAppsProxyLacrosFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* browser_context) const override;
  bool ServiceIsNULLWhileTesting() const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_LACROS_REMOTE_APPS_REMOTE_APPS_PROXY_LACROS_FACTORY_H_
