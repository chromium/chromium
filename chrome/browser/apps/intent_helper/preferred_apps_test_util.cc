// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"

#include "base/run_loop.h"
#include "base/scoped_observation.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"

namespace apps_util {

PreferredAppUpdateWaiter::PreferredAppUpdateWaiter(
    apps::PreferredAppsListHandle& handle,
    std::string app_id,
    bool is_preferred_app)
    : waiting_app_id_(std::move(app_id)),
      waiting_is_preferred_app_(is_preferred_app),
      preferred_apps_(handle) {
  observation_.Observe(&handle);
}

PreferredAppUpdateWaiter::~PreferredAppUpdateWaiter() = default;

void PreferredAppUpdateWaiter::Wait() {
  if (preferred_apps_->IsPreferredAppForSupportedLinks(waiting_app_id_) !=
      waiting_is_preferred_app_) {
    run_loop_.Run();
  }
}

void PreferredAppUpdateWaiter::OnPreferredAppChanged(const std::string& app_id,
                                                     bool is_preferred_app) {
  if (app_id == waiting_app_id_ &&
      is_preferred_app == waiting_is_preferred_app_) {
    run_loop_.Quit();
  }
}

void PreferredAppUpdateWaiter::OnPreferredAppsListWillBeDestroyed(
    apps::PreferredAppsListHandle* handle) {
  observation_.Reset();
}

void SetSupportedLinksPreferenceAndWait(Profile* profile,
                                        const std::string& app_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  PreferredAppUpdateWaiter update_waiter(proxy->PreferredAppsList(), app_id);
  proxy->SetSupportedLinksPreference(app_id);
  update_waiter.Wait();
}

void RemoveSupportedLinksPreferenceAndWait(Profile* profile,
                                           const std::string& app_id) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  PreferredAppUpdateWaiter update_waiter(proxy->PreferredAppsList(), app_id,
                                         /*is_preferred_app=*/false);
  proxy->RemoveSupportedLinksPreference(app_id);
  update_waiter.Wait();
}

}  // namespace apps_util
