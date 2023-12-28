// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"

#include <string>

#include "base/functional/bind.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"

namespace apps {

namespace {

bool ContainsExpectedScopeIntentFilter(GURL scope,
                                       const apps::AppUpdate& update) {
  apps::IntentFilterPtr expected =
      apps_util::MakeIntentFilterForUrlScope(scope);
  for (auto& intent_filter : update.IntentFilters()) {
    DCHECK(!intent_filter->IsBrowserFilter());
    if (*intent_filter == *expected) {
      return true;
    }
  }
  return false;
}

}  // namespace

AppTypeInitializationWaiter::AppTypeInitializationWaiter(Profile* profile,
                                                         apps::AppType app_type)
    : app_type_(app_type) {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();
  app_registry_cache_observer_.Observe(&cache);

  if (cache.IsAppTypeInitialized(app_type)) {
    run_loop_.Quit();
  }
}

AppTypeInitializationWaiter::~AppTypeInitializationWaiter() = default;

void AppTypeInitializationWaiter::Await(const base::Location& location) {
  run_loop_.Run(location);
}

void AppTypeInitializationWaiter::OnAppUpdate(const apps::AppUpdate& update) {}

void AppTypeInitializationWaiter::OnAppTypeInitialized(apps::AppType app_type) {
  if (app_type == app_type_) {
    run_loop_.Quit();
  }
}

void AppTypeInitializationWaiter::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

AppUpdateWaiter::AppUpdateWaiter(
    Profile* profile,
    const std::string& app_id,
    base::RepeatingCallback<bool(const apps::AppUpdate&)> condition)
    : app_id_(app_id), condition_(std::move(condition)) {
  apps::AppRegistryCache& cache =
      apps::AppServiceProxyFactory::GetForProfile(profile)->AppRegistryCache();
  app_registry_cache_observer_.Observe(&cache);
  cache.ForOneApp(app_id, [this](const apps::AppUpdate& update) {
    if (condition_.Run(update)) {
      run_loop_.Quit();
    }
  });
}

AppUpdateWaiter::~AppUpdateWaiter() = default;

void AppUpdateWaiter::Await(const base::Location& location) {
  run_loop_.Run(location);
}

void AppUpdateWaiter::OnAppUpdate(const apps::AppUpdate& update) {
  if (update.AppId() == app_id_ && condition_.Run(update)) {
    run_loop_.Quit();
  }
}

void AppUpdateWaiter::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}

AppReadinessWaiter::AppReadinessWaiter(
    Profile* profile,
    const std::string& app_id,
    base::RepeatingCallback<bool(apps::Readiness)> readiness_condition)
    : AppUpdateWaiter(profile,
                      app_id,
                      base::BindRepeating(
                          [](base::RepeatingCallback<bool(apps::Readiness)>
                                 readiness_condition,
                             const AppUpdate& update) {
                            return readiness_condition.Run(update.Readiness());
                          },
                          readiness_condition)) {}

AppReadinessWaiter::AppReadinessWaiter(Profile* profile,
                                       const std::string& app_id,
                                       apps::Readiness readiness)
    : AppReadinessWaiter(profile,
                         app_id,
                         base::BindRepeating(
                             [](apps::Readiness expected_readiness,
                                apps::Readiness readiness) {
                               return readiness == expected_readiness;
                             },
                             readiness)) {}

WebAppScopeWaiter::WebAppScopeWaiter(Profile* profile,
                                     const std::string& app_id,
                                     GURL scope)
    : AppUpdateWaiter(
          profile,
          app_id,
          base::BindRepeating(&ContainsExpectedScopeIntentFilter, scope)) {}

AppWindowModeWaiter::AppWindowModeWaiter(Profile* profile,
                                         const std::string& app_id,
                                         apps::WindowMode window_mode)
    : AppUpdateWaiter(
          profile,
          app_id,
          base::BindRepeating(
              [](apps::WindowMode expected_mode, const AppUpdate& update) {
                return update.WindowMode() == expected_mode;
              },
              window_mode)) {
  DCHECK_NE(window_mode, apps::WindowMode::kUnknown);
}

}  // namespace apps
