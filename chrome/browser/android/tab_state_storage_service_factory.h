// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_TAB_STATE_STORAGE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_TAB_STATE_STORAGE_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "chrome/browser/tab/tab_state_storage_service.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace tabs {

class TabStateStorageServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static TabStateStorageService* GetForProfile(Profile* profile);

  static TabStateStorageServiceFactory* GetInstance();

  TabStateStorageServiceFactory(const TabStateStorageServiceFactory&) = delete;
  void operator=(const TabStateStorageServiceFactory&) = delete;

 private:
  friend base::NoDestructor<TabStateStorageServiceFactory>;

  TabStateStorageServiceFactory();
  ~TabStateStorageServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace tabs

#endif  // CHROME_BROWSER_ANDROID_TAB_STATE_STORAGE_SERVICE_FACTORY_H_
