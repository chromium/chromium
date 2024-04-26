// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/webapk/webapk_install_service_factory.h"

#include "chrome/browser/android/webapk/webapk_install_service.h"

// static
WebApkInstallServiceFactory* WebApkInstallServiceFactory::GetInstance() {
  static base::NoDestructor<WebApkInstallServiceFactory> instance;
  return instance.get();
}

// static
WebApkInstallService* WebApkInstallServiceFactory::GetForBrowserContext(
    content::BrowserContext* context) {
  return static_cast<WebApkInstallService*>(
      GetInstance()->GetServiceForBrowserContext(context, true));
}

WebApkInstallServiceFactory::WebApkInstallServiceFactory()
    : ProfileKeyedServiceFactory(
          "WebApkInstallService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kRedirectedToOriginal)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kRedirectedToOriginal)
              .Build()) {}

WebApkInstallServiceFactory::~WebApkInstallServiceFactory() = default;

std::unique_ptr<KeyedService>
WebApkInstallServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  return std::make_unique<WebApkInstallService>(context);
}
