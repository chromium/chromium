// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_FACTORY_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace policy {
class DlpRulesManager;

// Initializes an instance of DlpRulesManager when a primary managed profile is
// being created, e.g. when managed user signs in.
class DlpRulesManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static DlpRulesManagerFactory* GetInstance();
  // Returns nullptr if there is no primary profile, e.g. the session is not
  // started.
  static DlpRulesManager* GetForPrimaryProfile();

 private:
  friend class base::NoDestructor<DlpRulesManagerFactory>;

  DlpRulesManagerFactory();
  ~DlpRulesManagerFactory() override = default;

  // BrowserStateKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace policy
#endif  // CHROME_BROWSER_CHROMEOS_POLICY_DLP_DLP_RULES_MANAGER_FACTORY_H_
