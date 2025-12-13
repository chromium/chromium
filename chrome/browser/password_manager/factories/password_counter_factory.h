// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_FACTORIES_PASSWORD_COUNTER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_FACTORIES_PASSWORD_COUNTER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace password_manager {
class PasswordCounter;
}  // namespace password_manager

class Profile;

// Creates instances of PasswordCounter per Profile.
class PasswordCounterFactory : public ProfileKeyedServiceFactory {
 public:
  static password_manager::PasswordCounter* GetForProfile(Profile* profile);

  static PasswordCounterFactory* GetInstance();

 private:
  friend base::NoDestructor<PasswordCounterFactory>;

  PasswordCounterFactory();
  ~PasswordCounterFactory() override;

  // ProfileKeyedServiceFactory overrides.
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_FACTORIES_PASSWORD_COUNTER_FACTORY_H_
