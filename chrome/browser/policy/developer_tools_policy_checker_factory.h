// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_CHECKER_FACTORY_H_
#define CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_CHECKER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace content {
class BrowserContext;
}

namespace policy {

class DeveloperToolsPolicyChecker;

class DeveloperToolsPolicyCheckerFactory : public ProfileKeyedServiceFactory {
 public:
  static DeveloperToolsPolicyCheckerFactory* GetInstance();
  static DeveloperToolsPolicyChecker* GetForBrowserContext(
      content::BrowserContext* context);

  DeveloperToolsPolicyCheckerFactory(
      const DeveloperToolsPolicyCheckerFactory&) = delete;
  DeveloperToolsPolicyCheckerFactory& operator=(
      const DeveloperToolsPolicyCheckerFactory&) = delete;

 private:
  friend class base::NoDestructor<DeveloperToolsPolicyCheckerFactory>;

  DeveloperToolsPolicyCheckerFactory();
  ~DeveloperToolsPolicyCheckerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace policy

#endif  // CHROME_BROWSER_POLICY_DEVELOPER_TOOLS_POLICY_CHECKER_FACTORY_H_
