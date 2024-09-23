// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_DESKTOP_H_
#define CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_DESKTOP_H_

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/apps/app_service/app_service_proxy_base.h"

// Avoid including this header file directly. Instead:
//  - for forward declarations, use app_service_proxy_forward.h
//  - for the full header, use app_service_proxy.h, which aliases correctly
//    based on the platform

class Profile;

namespace apps {

class PublisherHost;

// Singleton (per Profile) proxy and cache of an App Service's apps in Chrome
// browser.
//
// See components/services/app_service/README.md.
class AppServiceProxy : public AppServiceProxyBase {
 public:
  explicit AppServiceProxy(Profile* profile);
  AppServiceProxy(const AppServiceProxy&) = delete;
  AppServiceProxy& operator=(const AppServiceProxy&) = delete;
  ~AppServiceProxy() override;

 private:
  // For access to Initialize.
  friend class AppServiceProxyFactory;

  // apps::AppServiceProxyBase overrides:
  void Initialize() override;
  bool MaybeShowLaunchPreventionDialog(const apps::AppUpdate& update) override;

  std::unique_ptr<PublisherHost> publisher_host_;
};

}  // namespace apps

#endif  // CHROME_BROWSER_APPS_APP_SERVICE_APP_SERVICE_PROXY_DESKTOP_H_
