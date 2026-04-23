// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_GROUPS_ENTERPRISE_GROUPS_HANDLER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_GROUPS_ENTERPRISE_GROUPS_HANDLER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace policy {
class EnterpriseGroupsProfileHandler;
}  // namespace policy

namespace enterprise_groups {

class EnterpriseGroupsProfileHandlerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static EnterpriseGroupsProfileHandlerFactory* GetInstance();
  static policy::EnterpriseGroupsProfileHandler* GetForProfile(
      Profile* profile);

  EnterpriseGroupsProfileHandlerFactory(
      const EnterpriseGroupsProfileHandlerFactory&) = delete;
  EnterpriseGroupsProfileHandlerFactory& operator=(
      const EnterpriseGroupsProfileHandlerFactory&) = delete;
  ~EnterpriseGroupsProfileHandlerFactory() override;

 private:
  friend base::NoDestructor<EnterpriseGroupsProfileHandlerFactory>;

  // `BrowserContextKeyedServiceFactory` overrides.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  EnterpriseGroupsProfileHandlerFactory();
};

}  // namespace enterprise_groups

#endif  // CHROME_BROWSER_ENTERPRISE_GROUPS_ENTERPRISE_GROUPS_HANDLER_FACTORY_H_
