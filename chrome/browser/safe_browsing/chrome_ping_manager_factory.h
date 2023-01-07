// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_PING_MANAGER_FACTORY_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_PING_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/safe_browsing/core/browser/ping_manager.h"

namespace safe_browsing {

// Factory for creating the KeyedService PingManager for chrome. It returns null
// for incognito mode.
class ChromePingManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static ChromePingManagerFactory* GetInstance();
  static PingManager* GetForBrowserContext(content::BrowserContext* context);

 private:
  friend class base::NoDestructor<ChromePingManagerFactory>;

  ChromePingManagerFactory();
  ~ChromePingManagerFactory() override;

  // BrowserContextKeyedServiceFactory override:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  static bool ShouldFetchAccessTokenForReport(Profile* profile);
};

}  // namespace safe_browsing
#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_PING_MANAGER_FACTORY_H_
