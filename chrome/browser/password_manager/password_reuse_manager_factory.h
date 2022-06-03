// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_REUSE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_REUSE_MANAGER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace password_manager {
class PasswordReuseManager;
}

namespace content {
class BrowserContext;
}

// Creates instances of PasswordReuseManager per Profile.
class PasswordReuseManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  PasswordReuseManagerFactory();
  ~PasswordReuseManagerFactory() override;

  static PasswordReuseManagerFactory* GetInstance();
  static password_manager::PasswordReuseManager* GetForProfile(
      Profile* profile);

 private:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_REUSE_MANAGER_FACTORY_H_
