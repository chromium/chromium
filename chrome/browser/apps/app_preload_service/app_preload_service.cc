// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/app_preload_service.h"

#include <memory>

#include "base/bind.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_preload_service/app_preload_service_factory.h"
#include "chrome/browser/apps/app_preload_service/device_info_manager.h"
#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"
#include "chrome/browser/profiles/profile.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"

namespace {

// The pref dict is:
// {
//  ...
//  "apps.app_preload_service.state_manager": {
//    first_run_completed: <bool>,
//  },
//  ...
// }

static constexpr char kFirstLoginFlowCompletedKey[] =
    "first_login_flow_completed";

}  // namespace

namespace apps {

namespace prefs {
static constexpr char kApsStateManager[] =
    "apps.app_preload_service.state_manager";
}  // namespace prefs

AppPreloadService::AppPreloadService(Profile* profile)
    : profile_(profile),
      server_connector_(std::make_unique<AppPreloadServerConnector>()),
      device_info_manager_(std::make_unique<DeviceInfoManager>(profile)) {
  // Check to see if the service has been run before.
  auto is_first_run = GetStateManager().FindBool(kFirstLoginFlowCompletedKey);
  if (is_first_run == absl::nullopt) {
    // the first run completed key has not been set, kick off the initial app
    // installation flow.
    device_info_manager_->GetDeviceInfo(
        base::BindOnce(&AppPreloadService::StartAppInstallationForFirstLogin,
                       weak_ptr_factory_.GetWeakPtr()));
  }
}

AppPreloadService::~AppPreloadService() = default;

// static
AppPreloadService* AppPreloadService::Get(Profile* profile) {
  return AppPreloadServiceFactory::GetForProfile(profile);
}

// static
void AppPreloadService::RegisterProfilePrefs(
    user_prefs::PrefRegistrySyncable* registry) {
  registry->RegisterDictionaryPref(prefs::kApsStateManager);
}

void AppPreloadService::StartAppInstallationForFirstLogin(
    DeviceInfo device_info) {
  server_connector_->GetAppsForFirstLogin(
      device_info, profile_->GetURLLoaderFactory(),
      base::BindOnce(&AppPreloadService::OnGetAppsForFirstLoginCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppPreloadService::OnGetAppsForFirstLoginCompleted(
    std::vector<PreloadAppDefinition> apps) {
  ScopedDictPrefUpdate(profile_->GetPrefs(), prefs::kApsStateManager)
      ->Set(kFirstLoginFlowCompletedKey, true);

  if (check_first_pref_set_callback_) {
    std::move(check_first_pref_set_callback_).Run();
  }
}

const base::Value::Dict& AppPreloadService::GetStateManager() const {
  const base::Value::Dict& value =
      profile_->GetPrefs()->GetDict(prefs::kApsStateManager);
  return value;
}

}  // namespace apps
