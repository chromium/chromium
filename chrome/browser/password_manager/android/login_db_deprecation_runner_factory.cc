// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_manager/android/login_db_deprecation_runner_factory.h"

#include "base/feature_list.h"
#include "base/no_destructor.h"
#include "build/build_config.h"
#include "chrome/browser/password_manager/android/password_manager_util_bridge.h"
#include "chrome/browser/profiles/profile.h"
#include "components/password_manager/core/browser/export/login_db_deprecation_password_exporter.h"
#include "components/password_manager/core/browser/export/login_db_deprecation_runner.h"
#include "components/password_manager/core/browser/features/password_features.h"
#include "components/password_manager/core/browser/password_manager_buildflags.h"
#include "components/password_manager/core/browser/split_stores_and_local_upm.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"

LoginDbDeprecationRunnerFactory::LoginDbDeprecationRunnerFactory()
    : ProfileKeyedServiceFactory("LoginDbDeprecationRunnerFactory",
                                 ProfileSelections::BuildForRegularProfile()) {}
LoginDbDeprecationRunnerFactory::~LoginDbDeprecationRunnerFactory() = default;

LoginDbDeprecationRunnerFactory*
LoginDbDeprecationRunnerFactory::GetInstance() {
  static base::NoDestructor<LoginDbDeprecationRunnerFactory> instance;
  return instance.get();
}

password_manager::LoginDbDeprecationRunner*
LoginDbDeprecationRunnerFactory::GetForProfile(Profile* profile) {
  return static_cast<password_manager::LoginDbDeprecationRunner*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

std::unique_ptr<KeyedService>
LoginDbDeprecationRunnerFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
#if BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
  return nullptr;
#else
  Profile* profile = Profile::FromBrowserContext(context);
  PrefService* prefs = profile->GetPrefs();

  // If the client is already migrated there is no need for export.
  if (password_manager::UsesSplitStoresAndUPMForLocal(prefs)) {
    return nullptr;
  }

  if (!base::FeatureList::IsEnabled(
          password_manager::features::kLoginDbDeprecationAndroid)) {
    return nullptr;
  }

  if (prefs->GetBoolean(
          password_manager::prefs::kUpmUnmigratedPasswordsExported)) {
    // Since saving new passwords is disabled, one export is enough to guarantee
    // that all passwords have been preserved outside of the login database.
    return nullptr;
  }

  return std::make_unique<password_manager::LoginDbDeprecationRunner>(
      std::make_unique<password_manager::LoginDbDeprecationPasswordExporter>(
          profile->GetPrefs(), profile->GetPath()));
#endif  // BUILDFLAG(USE_LOGIN_DATABASE_AS_BACKEND)
}
