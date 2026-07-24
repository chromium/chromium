// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NOTEBOOKS_NOTEBOOKS_ELIGIBILITY_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NOTEBOOKS_NOTEBOOKS_ELIGIBILITY_SERVICE_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class Profile;

namespace content {
class BrowserContext;
}

namespace notebooks {
class NotebooksEligibilityService;

class NotebooksEligibilityServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the instance of NotebooksEligibilityService associated with this
  // profile. May return nullptr for profile types excluded by ProfileSelections
  // (such as guest sessions and system profiles).
  static NotebooksEligibilityService* GetForProfile(Profile* profile);

  static NotebooksEligibilityServiceFactory* GetInstance();

  NotebooksEligibilityServiceFactory(
      const NotebooksEligibilityServiceFactory&) = delete;
  NotebooksEligibilityServiceFactory& operator=(
      const NotebooksEligibilityServiceFactory&) = delete;

 private:
  friend base::NoDestructor<NotebooksEligibilityServiceFactory>;

  NotebooksEligibilityServiceFactory();
  ~NotebooksEligibilityServiceFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace notebooks

#endif  // CHROME_BROWSER_NOTEBOOKS_NOTEBOOKS_ELIGIBILITY_SERVICE_FACTORY_H_
