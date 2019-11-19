// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_GUEST_OS_GUEST_OS_SHARE_PATH_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_GUEST_OS_GUEST_OS_SHARE_PATH_FACTORY_H_

#include "base/macros.h"
#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace guest_os {

class GuestOsSharePath;

class GuestOsSharePathFactory : public BrowserContextKeyedServiceFactory {
 public:
  static GuestOsSharePath* GetForProfile(Profile* profile);
  static GuestOsSharePathFactory* GetInstance();

 private:
  friend class base::NoDestructor<GuestOsSharePathFactory>;

  GuestOsSharePathFactory();
  ~GuestOsSharePathFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(GuestOsSharePathFactory);
};

}  // namespace guest_os

#endif  // CHROME_BROWSER_CHROMEOS_GUEST_OS_GUEST_OS_SHARE_PATH_FACTORY_H_
