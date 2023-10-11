// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_FACTORY_H_
#define CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

namespace file_manager {

class VolumeManager;

// Factory to create VolumeManager.
class VolumeManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns VolumeManager instance.
  static VolumeManager* Get(content::BrowserContext* context);

  static VolumeManagerFactory* GetInstance();

  VolumeManagerFactory(const VolumeManagerFactory&) = delete;
  VolumeManagerFactory& operator=(const VolumeManagerFactory&) = delete;

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  // For Singleton.
  friend base::NoDestructor<VolumeManagerFactory>;

  VolumeManagerFactory();
  ~VolumeManagerFactory() override;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_FILE_MANAGER_VOLUME_MANAGER_FACTORY_H_
