// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_SYNC_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace content {
class BrowserContext;
}  // namespace content

class Profile;

namespace tab_groups {
class TabGroupSyncService;

// A factory to create a unique TabGroupSyncService.
class TabGroupSyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Gets the TabGroupSyncService for the profile. Returns null for incognito.
  // The caller is responsible for checking whether the feature is enabled.
  static TabGroupSyncService* GetForProfile(Profile* profile);

  // Gets the lazy singleton instance of TabGroupSyncServiceFactory.
  static TabGroupSyncServiceFactory* GetInstance();

  // Disallow copy/assign.
  TabGroupSyncServiceFactory(const TabGroupSyncServiceFactory&) = delete;
  void operator=(const TabGroupSyncServiceFactory&) = delete;

 private:
  friend base::NoDestructor<TabGroupSyncServiceFactory>;

  TabGroupSyncServiceFactory();
  ~TabGroupSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace tab_groups

#endif  // CHROME_BROWSER_TAB_GROUP_SYNC_TAB_GROUP_SYNC_SERVICE_FACTORY_H_
