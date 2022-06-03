// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "printing/buildflags/buildflags.h"

#if !BUILDFLAG(ENABLE_PRINT_PREVIEW) || BUILDFLAG(IS_CHROMEOS_ASH)
#error "Print Preview must be enabled / Not supported on ChromeOS"
#endif

class CloudPrintProxyService;
class Profile;

// Singleton that owns all CloudPrintProxyServices and associates them with
// Profiles. Listens for the Profile's destruction notification and cleans up
// the associated CloudPrintProxyService.
class CloudPrintProxyServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the CloudPrintProxyService for |profile|, creating if not yet
  // created.
  static CloudPrintProxyService* GetForProfile(Profile* profile);

  static CloudPrintProxyServiceFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<CloudPrintProxyServiceFactory>;

  CloudPrintProxyServiceFactory();
  ~CloudPrintProxyServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_PRINTING_CLOUD_PRINT_CLOUD_PRINT_PROXY_SERVICE_FACTORY_H_
