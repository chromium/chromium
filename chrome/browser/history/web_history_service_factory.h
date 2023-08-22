// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace history {
class WebHistoryService;
}

// Used for creating and fetching a per-profile instance of the
// WebHistoryService.
class WebHistoryServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Get the singleton instance of the factory.
  static WebHistoryServiceFactory* GetInstance();

  // Get the WebHistoryService for |profile|, creating one if needed.
  static history::WebHistoryService* GetForProfile(Profile* profile);

  WebHistoryServiceFactory(const WebHistoryServiceFactory&) = delete;
  WebHistoryServiceFactory& operator=(const WebHistoryServiceFactory&) = delete;

 protected:
  // Overridden from BrowserContextKeyedServiceFactory.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

 private:
  friend base::NoDestructor<WebHistoryServiceFactory>;

  WebHistoryServiceFactory();
  ~WebHistoryServiceFactory() override;
};

#endif  // CHROME_BROWSER_HISTORY_WEB_HISTORY_SERVICE_FACTORY_H_
