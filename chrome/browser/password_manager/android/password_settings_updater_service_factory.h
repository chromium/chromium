// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_SERVICE_FACTORY_H_

#include "base/memory/singleton.h"
#include "chrome/browser/password_manager/android/password_settings_updater_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/refcounted_keyed_service.h"
#include "components/keyed_service/core/service_access_type.h"
#include "content/public/browser/browser_context.h"

class Profile;

class PasswordSettingsUpdaterServiceFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static PasswordSettingsUpdaterService* GetForProfile(Profile* profile);

  static PasswordSettingsUpdaterServiceFactory* GetInstance();

  PasswordSettingsUpdaterServiceFactory(
      const PasswordSettingsUpdaterServiceFactory&) = delete;
  PasswordSettingsUpdaterServiceFactory operator=(
      const PasswordSettingsUpdaterServiceFactory&) = delete;

 private:
  friend base::DefaultSingletonTraits<PasswordSettingsUpdaterServiceFactory>;

  PasswordSettingsUpdaterServiceFactory();
  ~PasswordSettingsUpdaterServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsNULLWhileTesting() const override;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_PASSWORD_SETTINGS_UPDATER_SERVICE_FACTORY_H_
