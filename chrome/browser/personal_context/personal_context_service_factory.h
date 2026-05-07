// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERSONAL_CONTEXT_PERSONAL_CONTEXT_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PERSONAL_CONTEXT_PERSONAL_CONTEXT_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace personal_context {
class PersonalContextService;
}  // namespace personal_context

class PersonalContextServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static personal_context::PersonalContextService* GetForProfile(
      Profile* profile);
  static PersonalContextServiceFactory* GetInstance();

  PersonalContextServiceFactory(
      const PersonalContextServiceFactory&) = delete;
  PersonalContextServiceFactory& operator=(
      const PersonalContextServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PersonalContextServiceFactory>;

  PersonalContextServiceFactory();
  ~PersonalContextServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PERSONAL_CONTEXT_PERSONAL_CONTEXT_SERVICE_FACTORY_H_
