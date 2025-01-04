// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_LOGIN_DB_DEPRECATION_RUNNER_FACTORY_H_
#define CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_LOGIN_DB_DEPRECATION_RUNNER_FACTORY_H_

#include "base/no_destructor.h"
#include "base/types/pass_key.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/password_manager/core/browser/export/login_db_deprecation_runner.h"
#include "content/public/browser/browser_context.h"

// Creates the `LoginDbDeprecationRunner` which runs the pre-deprecation
// export for unmigrated passwords on Android.
class LoginDbDeprecationRunnerFactory : public ProfileKeyedServiceFactory {
 public:
  static LoginDbDeprecationRunnerFactory* GetInstance();
  static password_manager::LoginDbDeprecationRunner* GetForProfile(
      Profile* profile);

 private:
  friend class base::NoDestructor<LoginDbDeprecationRunnerFactory>;

  LoginDbDeprecationRunnerFactory();
  ~LoginDbDeprecationRunnerFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;

  base::RepeatingCallback<bool()> internal_backend_checker_;
};

#endif  // CHROME_BROWSER_PASSWORD_MANAGER_ANDROID_LOGIN_DB_DEPRECATION_RUNNER_FACTORY_H_
