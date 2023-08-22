// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class NtpBackgroundService;
class Profile;

class NtpBackgroundServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the NtpBackgroundService for |profile|.
  static NtpBackgroundService* GetForProfile(Profile* profile);

  static NtpBackgroundServiceFactory* GetInstance();

  NtpBackgroundServiceFactory(const NtpBackgroundServiceFactory&) = delete;
  NtpBackgroundServiceFactory& operator=(const NtpBackgroundServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<NtpBackgroundServiceFactory>;

  NtpBackgroundServiceFactory();
  ~NtpBackgroundServiceFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_NTP_BACKGROUND_SERVICE_FACTORY_H_
