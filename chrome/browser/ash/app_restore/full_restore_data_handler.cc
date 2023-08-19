// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_restore/full_restore_data_handler.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/app_restore/full_restore_read_handler.h"
#include "components/app_restore/full_restore_save_handler.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"

namespace ash::full_restore {

FullRestoreDataHandler::FullRestoreDataHandler(Profile* profile)
    : profile_(profile) {
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile_));
  app_registry_cache_observer_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile_)
           ->AppRegistryCache());
}

FullRestoreDataHandler::~FullRestoreDataHandler() = default;

void FullRestoreDataHandler::OnAppUpdate(const apps::AppUpdate& update) {
  if (!update.ReadinessChanged() ||
      apps_util::IsInstalled(update.Readiness())) {
    return;
  }

  // If the user uninstalls an app, then installs it again at the system
  // startup phase, its restore data will be removed if the app isn't reopened.
  ::full_restore::FullRestoreReadHandler* read_handler =
      ::full_restore::FullRestoreReadHandler::GetInstance();
  read_handler->RemoveApp(profile_->GetPath(), update.AppId());

  ::full_restore::FullRestoreSaveHandler::GetInstance()->RemoveApp(
      profile_->GetPath(), update.AppId());
}

void FullRestoreDataHandler::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

}  // namespace ash::full_restore
