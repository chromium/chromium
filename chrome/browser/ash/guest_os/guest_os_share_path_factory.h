// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SHARE_PATH_FACTORY_H_
#define CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SHARE_PATH_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace guest_os {

class GuestOsSharePath;

class GuestOsSharePathFactory : public ProfileKeyedServiceFactory {
 public:
  static GuestOsSharePath* GetForProfile(Profile* profile);
  static GuestOsSharePathFactory* GetInstance();

  GuestOsSharePathFactory(const GuestOsSharePathFactory&) = delete;
  GuestOsSharePathFactory& operator=(const GuestOsSharePathFactory&) = delete;

 private:
  friend class base::NoDestructor<GuestOsSharePathFactory>;

  GuestOsSharePathFactory();
  ~GuestOsSharePathFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_ASH_GUEST_OS_GUEST_OS_SHARE_PATH_FACTORY_H_
