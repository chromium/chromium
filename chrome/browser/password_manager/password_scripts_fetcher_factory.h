// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_SCRIPTS_FETCHER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_SCRIPTS_FETCHER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace password_manager {
class PasswordScriptsFetcher;
}

namespace content {
class BrowserContext;
}

// Creates instances of |PasswordScriptsFetcher| per |BrowserContext|.
class PasswordScriptsFetcherFactory : public BrowserContextKeyedServiceFactory {
 public:
  PasswordScriptsFetcherFactory();
  ~PasswordScriptsFetcherFactory() override;

  static PasswordScriptsFetcherFactory* GetInstance();
  static password_manager::PasswordScriptsFetcher* GetForBrowserContext(
      content::BrowserContext* browser_context);

 private:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* browser_context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_SCRIPTS_FETCHER_FACTORY_H_
