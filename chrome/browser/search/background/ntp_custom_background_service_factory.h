// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "content/public/browser/browser_context.h"

class NtpCustomBackgroundService;
class Profile;

class NtpCustomBackgroundServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static NtpCustomBackgroundService* GetForProfile(Profile* profile);
  static NtpCustomBackgroundServiceFactory* GetInstance();

  NtpCustomBackgroundServiceFactory(const NtpCustomBackgroundServiceFactory&) =
      delete;
  NtpCustomBackgroundServiceFactory& operator=(
      const NtpCustomBackgroundServiceFactory&) = delete;

 private:
  friend base::NoDestructor<NtpCustomBackgroundServiceFactory>;

  NtpCustomBackgroundServiceFactory();
  ~NtpCustomBackgroundServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_CUSTOM_BACKGROUND_SERVICE_FACTORY_H_
