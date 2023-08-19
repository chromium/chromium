// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/early_prefs/early_prefs_export_service_factory.h"

#include "ash/constants/ash_features.h"
#include "base/check_is_test.h"
#include "base/feature_list.h"
#include "base/path_service.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_paths.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "chromeos/ash/components/osauth/impl/login_screen_auth_policy_connector.h"
#include "chromeos/ash/components/osauth/public/auth_parts.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash {

// static
EarlyPrefsExportServiceFactory* EarlyPrefsExportServiceFactory::GetInstance() {
  return base::Singleton<EarlyPrefsExportServiceFactory>::get();
}

EarlyPrefsExportServiceFactory::EarlyPrefsExportServiceFactory()
    : BrowserContextKeyedServiceFactory(
          "EarlyPrefsExportService",
          BrowserContextDependencyManager::GetInstance()) {}

EarlyPrefsExportServiceFactory::~EarlyPrefsExportServiceFactory() = default;

std::unique_ptr<KeyedService>
EarlyPrefsExportServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  auto* profile = Profile::FromBrowserContext(context);

  PrefService* user_prefs = profile->GetPrefs();
  auto* primary_user = user_manager::UserManager::Get()->GetPrimaryUser();
  CHECK(primary_user);
  base::FilePath early_prefs_dir;
  bool success = base::PathService::Get(chrome::DIR_CHROMEOS_HOMEDIR_MOUNT,
                                        &early_prefs_dir);
  CHECK(success);
  early_prefs_dir = early_prefs_dir.Append(primary_user->username_hash());

  std::unique_ptr<KeyedService> service =
      std::make_unique<EarlyPrefsExportService>(early_prefs_dir, user_prefs);
  return service;
}

content::BrowserContext* EarlyPrefsExportServiceFactory::GetBrowserContextToUse(
    content::BrowserContext* context) const {
  if (!base::FeatureList::IsEnabled(features::kEnableEarlyPrefs)) {
    return nullptr;
  }

  if (!context || context->IsOffTheRecord()) {
    return nullptr;
  }

  auto* profile = Profile::FromBrowserContext(context);
  if (profile->AsTestingProfile() || profile->IsGuestSession() ||
      !ProfileHelper::IsUserProfile(profile)) {
    return nullptr;
  }

  return context;
}

bool EarlyPrefsExportServiceFactory::ServiceIsCreatedWithBrowserContext()
    const {
  return true;
}

}  // namespace ash
