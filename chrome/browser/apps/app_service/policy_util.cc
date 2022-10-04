// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/policy_util.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_list_prefs.h"
#include "components/crx_file/id_util.h"
#include "components/services/app_service/public/cpp/app_update.h"

namespace apps_util {

bool IsChromeAppPolicyId(const std::string& policy_id) {
  return crx_file::id_util::IdIsValid(policy_id);
}

bool IsArcAppPolicyId(const std::string& policy_id) {
  return policy_id.find('.') != std::string::npos &&
         !IsWebAppPolicyId(policy_id);
}

bool IsWebAppPolicyId(const std::string& policy_id) {
  return GURL{policy_id}.is_valid();
}

std::string TransformRawPolicyId(const std::string& raw_policy_id) {
  if (const GURL raw_policy_id_gurl{raw_policy_id};
      raw_policy_id_gurl.is_valid()) {
    return raw_policy_id_gurl.spec();
  }

  return raw_policy_id;
}

absl::optional<std::string> GetAppIdFromPolicyId(Profile* profile,
                                                 const std::string& policy_id) {
  // AppService might be absent in some cases, e.g. Arc++ Kiosk mode.
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    absl::optional<std::string> app_id;
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->AppRegistryCache()
        .ForEachApp([&policy_id, &app_id](const apps::AppUpdate& update) {
          if (base::Contains(update.PolicyIds(), policy_id)) {
            DCHECK(!app_id);
            app_id = update.AppId();
          }
        });

    return app_id;
  }

  if (IsChromeAppPolicyId(policy_id)) {
    return policy_id;
  }

  if (IsArcAppPolicyId(policy_id)) {
    auto* arc_prefs = ArcAppListPrefs::Get(profile);
    if (!arc_prefs) {
      return {};
    }
    std::string app_id = arc_prefs->GetAppIdByPackageName(policy_id);
    if (app_id.empty()) {
      return {};
    }
    return app_id;
  }

  return {};
}

absl::optional<std::vector<std::string>> GetPolicyIdsFromAppId(
    Profile* profile,
    const std::string& app_id) {
  // AppService might be absent in some cases, e.g. Arc++ Kiosk mode.
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    absl::optional<std::vector<std::string>> policy_ids;
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->AppRegistryCache()
        .ForOneApp(app_id, [&policy_ids](const apps::AppUpdate& update) {
          policy_ids = update.PolicyIds();
        });

    return policy_ids;
  }

  // Handle Arc++ ids
  if (auto* arc_prefs = ArcAppListPrefs::Get(profile)) {
    if (auto app_info = arc_prefs->GetApp(app_id)) {
      return {{app_info->package_name}};
    }
  }

  // Handle Chrome App ids
  return {{app_id}};
}

}  // namespace apps_util
