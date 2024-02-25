// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_EVENT_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class TabGroupsEventRouter;

// The factory responsible for creating the event router for the tabGroups API.
class TabGroupsEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the TabGroupsEventRouter for |profile|, creating it if
  // it is not yet created.
  static TabGroupsEventRouter* Get(content::BrowserContext* context);

  // Returns the TabGroupsEventRouterFactory instance.
  static TabGroupsEventRouterFactory* GetInstance();

  TabGroupsEventRouterFactory(const TabGroupsEventRouterFactory&) = delete;
  TabGroupsEventRouterFactory& operator=(const TabGroupsEventRouterFactory&) =
      delete;

 protected:
  friend base::NoDestructor<TabGroupsEventRouterFactory>;

 private:
  TabGroupsEventRouterFactory();
  ~TabGroupsEventRouterFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_TAB_GROUPS_TAB_GROUPS_EVENT_ROUTER_FACTORY_H_
