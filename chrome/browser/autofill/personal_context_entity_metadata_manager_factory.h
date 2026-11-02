// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_PERSONAL_CONTEXT_ENTITY_METADATA_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_PERSONAL_CONTEXT_ENTITY_METADATA_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class KeyedService;
class Profile;

namespace content {
class BrowserContext;
}

namespace autofill {

class PersonalContextEntityMetadataManager;

// TODO(crbug.com/501036619): Remove PersonalContextEntityMetadataManagerFactory
// from the autofill namespace.
class PersonalContextEntityMetadataManagerFactory
    : public ProfileKeyedServiceFactory {
 public:
  static PersonalContextEntityMetadataManager* GetForProfile(Profile* profile);
  static PersonalContextEntityMetadataManagerFactory* GetInstance();

  PersonalContextEntityMetadataManagerFactory(
      const PersonalContextEntityMetadataManagerFactory&) = delete;
  PersonalContextEntityMetadataManagerFactory& operator=(
      const PersonalContextEntityMetadataManagerFactory&) = delete;

 private:
  friend base::NoDestructor<PersonalContextEntityMetadataManagerFactory>;

  PersonalContextEntityMetadataManagerFactory();
  ~PersonalContextEntityMetadataManagerFactory() override;

  // ProfileKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_PERSONAL_CONTEXT_ENTITY_METADATA_MANAGER_FACTORY_H_
