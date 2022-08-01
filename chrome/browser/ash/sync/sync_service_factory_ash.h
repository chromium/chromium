// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_SERVICE_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_SERVICE_FACTORY_ASH_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ash {

class SyncServiceAsh;

class SyncServiceFactoryAsh : public ProfileKeyedServiceFactory {
 public:
  static SyncServiceAsh* GetForProfile(Profile* profile);
  static SyncServiceFactoryAsh* GetInstance();

  SyncServiceFactoryAsh(const SyncServiceFactoryAsh& other) = delete;
  SyncServiceFactoryAsh& operator=(const SyncServiceFactoryAsh& other) = delete;

 private:
  friend struct base::DefaultSingletonTraits<SyncServiceFactoryAsh>;

  SyncServiceFactoryAsh();
  ~SyncServiceFactoryAsh() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_SERVICE_FACTORY_ASH_H_
