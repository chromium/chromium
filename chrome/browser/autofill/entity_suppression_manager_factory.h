// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_ENTITY_SUPPRESSION_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_ENTITY_SUPPRESSION_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace autofill {

class EntitySuppressionManager;

class EntitySuppressionManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static EntitySuppressionManager* GetForProfile(Profile* profile);
  static EntitySuppressionManagerFactory* GetInstance();

  EntitySuppressionManagerFactory(const EntitySuppressionManagerFactory&) =
      delete;
  EntitySuppressionManagerFactory& operator=(
      const EntitySuppressionManagerFactory&) = delete;

 private:
  friend base::NoDestructor<EntitySuppressionManagerFactory>;

  EntitySuppressionManagerFactory();
  ~EntitySuppressionManagerFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_ENTITY_SUPPRESSION_MANAGER_FACTORY_H_
