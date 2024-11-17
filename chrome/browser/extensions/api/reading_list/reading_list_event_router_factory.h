// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EVENT_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class ReadingListEventRouter;

// The factory responsible for creating the per-profile event router for the
// ReadingList API.
class ReadingListEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  // Given a browser context, returns the corresponding ReadingListEventRouter.
  static ReadingListEventRouter* GetForBrowserContext(
      content::BrowserContext* context);

  // Retrieves the singleton instance of the factory.
  static ReadingListEventRouterFactory* GetInstance();

  ReadingListEventRouterFactory(const ReadingListEventRouterFactory&) = delete;
  ReadingListEventRouterFactory& operator=(
      const ReadingListEventRouterFactory&) = delete;

 private:
  friend base::NoDestructor<ReadingListEventRouterFactory>;

  ReadingListEventRouterFactory();
  ~ReadingListEventRouterFactory() override;

  // ProfileKeyedServiceFactory overrides.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_READING_LIST_READING_LIST_EVENT_ROUTER_FACTORY_H_
