// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/chromeos_link_capturing_user_choice.h"

#include <string>
#include <utility>

#include "base/check.h"
#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/intent_filter.h"
#include "components/services/app_service/public/cpp/intent_filter_util.h"
#include "components/services/app_service/public/cpp/preferred_apps_list_handle.h"

namespace apps {

namespace {

IntentFilters GetSupportedLinkFilters(AppServiceProxy& proxy,
                                      const std::string& app_id) {
  IntentFilters filters;
  proxy.AppRegistryCache().ForOneApp(
      app_id, [&app_id, &filters](const AppUpdate& update) {
        for (auto& filter : update.IntentFilters()) {
          if (apps_util::IsSupportedLinkForApp(app_id, filter)) {
            filters.push_back(std::move(filter));
          }
        }
      });
  return filters;
}

}  // namespace

void SetSupportedLinksPreferenceByUser(Profile* profile,
                                       const std::string& app_id,
                                       bool open_in_app) {
  CHECK(!app_id.empty());
  AppServiceProxy* proxy = AppServiceProxyFactory::GetForProfile(profile);
  PreferredAppsListHandle& preferred_apps = proxy->PreferredAppsList();
  const IntentFilters link_filters = GetSupportedLinkFilters(*proxy, app_id);

  // Apps that App Service removes as preferred apps because they overlap
  // with `app_id`.
  base::flat_set<std::string> displaced_app_ids;
  if (open_in_app) {
    for (const std::string& other_app_id :
         preferred_apps.FindPreferredAppsForFilters(app_id, link_filters)) {
      if (preferred_apps.IsPreferredAppForSupportedLinks(other_app_id)) {
        displaced_app_ids.insert(other_app_id);
      }
    }
    // The change is synchronous if the preferred apps list is initialized.
    // If navigation capturing is on by default, it does nothing if an
    // overlapping preferred ARC app or System Web App blocks it.
    proxy->SetSupportedLinksPreference(app_id);
    base::EraseIf(
        displaced_app_ids, [&preferred_apps](const std::string& other_app_id) {
          return preferred_apps.IsPreferredAppForSupportedLinks(other_app_id);
        });
  } else {
    proxy->RemoveSupportedLinksPreference(app_id);
  }

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  // An app without supported links cannot capture links, so record nothing.
  if (!provider || link_filters.empty()) {
    return;
  }
  // Record "Open in app" only if App Service applied it. The command does
  // nothing for apps that are not installed web apps.
  if (!open_in_app || preferred_apps.IsPreferredAppForSupportedLinks(app_id)) {
    provider->scheduler().SetUserLinkCapturingChoiceOnChromeOs(
        app_id, open_in_app, base::DoNothing());
  }
  for (const std::string& displaced_app_id : displaced_app_ids) {
    provider->scheduler().SetUserLinkCapturingChoiceOnChromeOs(
        displaced_app_id, /*open_in_app=*/false, base::DoNothing());
  }
}

}  // namespace apps
