// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_REUSE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_REUSE_MANAGER_FACTORY_H_

#include <memory>

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace password_manager {
class PasswordReuseManager;
}

namespace content {
class BrowserContext;
}

// Creates instances of PasswordReuseManager per Profile.
class PasswordReuseManagerFactory : public ProfileKeyedServiceFactory {
 public:
  PasswordReuseManagerFactory();
  ~PasswordReuseManagerFactory() override;

  static PasswordReuseManagerFactory* GetInstance();
  static password_manager::PasswordReuseManager* GetForProfile(
      Profile* profile);

 private:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_REUSE_MANAGER_FACTORY_H_
