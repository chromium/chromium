// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SETTINGS_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SETTINGS_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/password_manager_settings_service.h"
#include "content/public/browser/browser_context.h"

class Profile;

class PasswordManagerSettingsServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static password_manager::PasswordManagerSettingsService* GetForProfile(
      Profile* profile);

  static PasswordManagerSettingsServiceFactory* GetInstance();

  PasswordManagerSettingsServiceFactory(
      const PasswordManagerSettingsServiceFactory&) = delete;
  PasswordManagerSettingsServiceFactory operator=(
      const PasswordManagerSettingsServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PasswordManagerSettingsServiceFactory>;

  PasswordManagerSettingsServiceFactory();
  ~PasswordManagerSettingsServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_PASSWORD_MANAGER_SETTINGS_SERVICE_FACTORY_H_
