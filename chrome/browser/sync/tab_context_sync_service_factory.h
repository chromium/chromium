// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_TAB_CONTEXT_SYNC_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SYNC_TAB_CONTEXT_SYNC_SERVICE_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace sync_tab_context {
class TabContextSyncService;
}  // namespace sync_tab_context

class TabContextSyncServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static TestingFactory GetDefaultFactory();
  static sync_tab_context::TabContextSyncService* GetForProfile(
      Profile* profile);
  static TabContextSyncServiceFactory* GetInstance();

  TabContextSyncServiceFactory(const TabContextSyncServiceFactory&) = delete;
  TabContextSyncServiceFactory& operator=(const TabContextSyncServiceFactory&) =
      delete;

 private:
  friend base::NoDestructor<TabContextSyncServiceFactory>;

  TabContextSyncServiceFactory();
  ~TabContextSyncServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_SYNC_TAB_CONTEXT_SYNC_SERVICE_FACTORY_H_
