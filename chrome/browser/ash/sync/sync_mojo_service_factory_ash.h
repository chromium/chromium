// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYNC_SYNC_MOJO_SERVICE_FACTORY_ASH_H_
#define CHROME_BROWSER_ASH_SYNC_SYNC_MOJO_SERVICE_FACTORY_ASH_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace ash {

class SyncMojoServiceAsh;

class SyncMojoServiceFactoryAsh : public ProfileKeyedServiceFactory {
 public:
  static SyncMojoServiceAsh* GetForProfile(Profile* profile);
  static SyncMojoServiceFactoryAsh* GetInstance();

  SyncMojoServiceFactoryAsh(const SyncMojoServiceFactoryAsh& other) = delete;
  SyncMojoServiceFactoryAsh& operator=(const SyncMojoServiceFactoryAsh& other) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<SyncMojoServiceFactoryAsh>;

  SyncMojoServiceFactoryAsh();
  ~SyncMojoServiceFactoryAsh() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYNC_SYNC_MOJO_SERVICE_FACTORY_ASH_H_
