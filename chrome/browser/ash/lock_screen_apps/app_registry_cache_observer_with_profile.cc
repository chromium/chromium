// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/lock_screen_apps/app_registry_cache_observer_with_profile.h"

#include "base/check.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"

class Profile;

namespace apps {

class AppUpdate;

AppRegistryCacheObserverWithProfile::AppRegistryCacheObserverWithProfile(
    Delegate* delegate,
    Profile* profile)
    : delegate_(delegate), profile_(profile) {
  DCHECK(delegate);
  DCHECK(profile);
  DCHECK(
      apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile));
  app_registry_observation_.Observe(
      &apps::AppServiceProxyFactory::GetForProfile(profile)
           ->AppRegistryCache());
}

AppRegistryCacheObserverWithProfile::~AppRegistryCacheObserverWithProfile() {
  app_registry_observation_.Reset();
}

void AppRegistryCacheObserverWithProfile::OnAppUpdate(
    const apps::AppUpdate& update) {
  delegate_->OnAppUpdate(update, profile_);
}

void AppRegistryCacheObserverWithProfile::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_observation_.Reset();
}

}  // namespace apps
