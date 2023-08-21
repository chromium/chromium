// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace file_manager {

class EventRouter;

class EventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the EventRouter for |profile|, creating it if
  // it is not yet created.
  static EventRouter* GetForProfile(Profile* profile);

  // Returns the EventRouterFactory instance.
  static EventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  friend base::NoDestructor<EventRouterFactory>;

  EventRouterFactory();
  ~EventRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace file_manager

#endif  // CHROME_BROWSER_ASH_EXTENSIONS_FILE_MANAGER_EVENT_ROUTER_FACTORY_H_
