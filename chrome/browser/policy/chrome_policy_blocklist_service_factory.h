// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_CHROME_POLICY_BLOCKLIST_SERVICE_FACTORY_H_
#define CHROME_BROWSER_POLICY_CHROME_POLICY_BLOCKLIST_SERVICE_FACTORY_H_

#include <memory>

#include "base/memory/singleton.h"
#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/policy/core/browser/url_list/policy_blocklist_service.h"

namespace content {
class BrowserContext;
}

// Factory for PolicyBlocklistService in Chrome.
class ChromePolicyBlocklistServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ChromePolicyBlocklistServiceFactory* GetInstance();

  static PolicyBlocklistService* GetForProfile(Profile* profile);

  ChromePolicyBlocklistServiceFactory(
      const ChromePolicyBlocklistServiceFactory&) = delete;
  ChromePolicyBlocklistServiceFactory& operator=(
      const ChromePolicyBlocklistServiceFactory&) = delete;

 private:
  friend base::NoDestructor<ChromePolicyBlocklistServiceFactory>;

  ChromePolicyBlocklistServiceFactory();
  ~ChromePolicyBlocklistServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_POLICY_CHROME_POLICY_BLOCKLIST_SERVICE_FACTORY_H_
