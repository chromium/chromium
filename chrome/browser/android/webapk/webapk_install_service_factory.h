// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class WebApkInstallService;

// Factory for creating WebApkInstallService. Installing WebAPKs from incognito
// is unsupported.
class WebApkInstallServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static WebApkInstallServiceFactory* GetInstance();
  static WebApkInstallService* GetForBrowserContext(
      content::BrowserContext* context);

  WebApkInstallServiceFactory(const WebApkInstallServiceFactory&) = delete;
  WebApkInstallServiceFactory& operator=(const WebApkInstallServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<WebApkInstallServiceFactory>;

  WebApkInstallServiceFactory();
  ~WebApkInstallServiceFactory() override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_ANDROID_WEBAPK_WEBAPK_INSTALL_SERVICE_FACTORY_H_
