// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/policy_util.h"

#include <array>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "components/crx_file/id_util.h"
#include "components/services/app_service/public/cpp/app_update.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/app_list/arc/arc_app_list_prefs.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace apps_util {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)

// This mapping excludes SWAs not included in official builds (like SAMPLE).
// These app Id constants need to be kept in sync with java/com/
// google/chrome/cros/policyconverter/ChromePolicySettingsProcessor.java
constexpr auto kSystemWebAppsMapping =
    base::MakeFixedFlatMap<base::StringPiece, ash::SystemWebAppType>(
        {{"file_manager", ash::SystemWebAppType::FILE_MANAGER},
         {"settings", ash::SystemWebAppType::SETTINGS},
         {"camera", ash::SystemWebAppType::CAMERA},
         {"terminal", ash::SystemWebAppType::TERMINAL},
         {"media", ash::SystemWebAppType::MEDIA},
         {"help", ash::SystemWebAppType::HELP},
         {"print_management", ash::SystemWebAppType::PRINT_MANAGEMENT},
         {"scanning", ash::SystemWebAppType::SCANNING},
         {"diagnostics", ash::SystemWebAppType::DIAGNOSTICS},
         {"connectivity_diagnostics",
          ash::SystemWebAppType::CONNECTIVITY_DIAGNOSTICS},
         {"eche", ash::SystemWebAppType::ECHE},
         {"crosh", ash::SystemWebAppType::CROSH},
         {"personalization", ash::SystemWebAppType::PERSONALIZATION},
         {"shortcut_customization",
          ash::SystemWebAppType::SHORTCUT_CUSTOMIZATION},
         {"shimless_rma", ash::SystemWebAppType::SHIMLESS_RMA},
         {"demo_mode", ash::SystemWebAppType::DEMO_MODE},
         {"os_feedback", ash::SystemWebAppType::OS_FEEDBACK},
         {"projector", ash::SystemWebAppType::PROJECTOR},
         {"os_url_handler", ash::SystemWebAppType::OS_URL_HANDLER},
         {"firmware_update", ash::SystemWebAppType::FIRMWARE_UPDATE},
         {"os_flags", ash::SystemWebAppType::OS_FLAGS},
         {"face_ml", ash::SystemWebAppType::FACE_ML}});

constexpr ash::SystemWebAppType GetMaxSystemWebAppType() {
  return base::ranges::max(kSystemWebAppsMapping, base::ranges::less{},
                           [](const auto& systemWebAppMappingPair) {
                             return systemWebAppMappingPair.second;
                           })
      .second;
}

static_assert(GetMaxSystemWebAppType() == ash::SystemWebAppType::kMaxValue,
              "Not all SWA types are listed in |system_web_apps_mapping|.");

#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

// Note that this mapping lists only selected Preinstalled Web Apps
// actively used in policies and is not meant to be exhaustive.
// These app Id constants need to be kept in sync with java/com/
// google/chrome/cros/policyconverter/ChromePolicySettingsProcessor.java
constexpr auto kPreinstalledWebAppsMapping =
    base::MakeFixedFlatMap<base::StringPiece, base::StringPiece>(
        {{"cursive", web_app::kCursiveAppId},
         {"canvas", web_app::kCanvasAppId}});

absl::optional<base::flat_map<base::StringPiece, base::StringPiece>>&
GetPreinstalledWebAppsMappingForTesting() {
  static base::NoDestructor<
      absl::optional<base::flat_map<base::StringPiece, base::StringPiece>>>
      preinstalled_web_apps_mapping_for_testing;
  return *preinstalled_web_apps_mapping_for_testing;
}

}  // namespace

bool IsSupportedAppTypePolicyId(base::StringPiece policy_id) {
  return IsChromeAppPolicyId(policy_id) ||
#if BUILDFLAG(IS_CHROMEOS_ASH)
         IsArcAppPolicyId(policy_id) || IsSystemWebAppPolicyId(policy_id) ||
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
         IsWebAppPolicyId(policy_id) || IsPreinstalledWebAppPolicyId(policy_id);
}

bool IsChromeAppPolicyId(base::StringPiece policy_id) {
  return crx_file::id_util::IdIsValid(policy_id);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsArcAppPolicyId(base::StringPiece policy_id) {
  return policy_id.find('.') != base::StringPiece::npos &&
         !IsWebAppPolicyId(policy_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool IsWebAppPolicyId(base::StringPiece policy_id) {
  return GURL{policy_id}.is_valid();
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsSystemWebAppPolicyId(base::StringPiece policy_id) {
  return base::Contains(kSystemWebAppsMapping, policy_id);
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

bool IsPreinstalledWebAppPolicyId(base::StringPiece policy_id) {
  if (auto& mapping = GetPreinstalledWebAppsMappingForTesting()) {  // IN-TEST
    return base::Contains(*mapping, policy_id);
  }
  return base::Contains(kPreinstalledWebAppsMapping, policy_id);
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
  // TODO(b/240493670): Revisit this after app service is available in Kiosk.
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
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
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return {};
}

absl::optional<std::vector<std::string>> GetPolicyIdsFromAppId(
    Profile* profile,
    const std::string& app_id) {
  // AppService might be absent in some cases, e.g. Arc++ Kiosk mode.
  // TODO(b/240493670): Revisit this after app service is available in Kiosk.
  if (apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    absl::optional<std::vector<std::string>> policy_ids;
    apps::AppServiceProxyFactory::GetForProfile(profile)
        ->AppRegistryCache()
        .ForOneApp(app_id, [&policy_ids](const apps::AppUpdate& update) {
          policy_ids = update.PolicyIds();
        });

    return policy_ids;
  }

#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Handle Arc++ ids
  if (auto* arc_prefs = ArcAppListPrefs::Get(profile)) {
    if (auto app_info = arc_prefs->GetApp(app_id)) {
      return {{app_info->package_name}};
    }
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // Handle Chrome App ids
  return {{app_id}};
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
absl::optional<base::StringPiece> GetPolicyIdForSystemWebAppType(
    ash::SystemWebAppType swa_type) {
  for (const auto& [policy_id, mapped_swa_type] : kSystemWebAppsMapping) {
    if (mapped_swa_type == swa_type) {
      return policy_id;
    }
  }
  return {};
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

absl::optional<base::StringPiece> GetPolicyIdForPreinstalledWebApp(
    base::StringPiece app_id) {
  if (const auto& test_mapping = GetPreinstalledWebAppsMappingForTesting()) {
    for (const auto& [policy_id, mapped_app_id] : *test_mapping) {
      if (mapped_app_id == app_id) {
        return policy_id;
      }
    }
    return {};
  }

  for (const auto& [policy_id, mapped_app_id] : kPreinstalledWebAppsMapping) {
    if (mapped_app_id == app_id) {
      return policy_id;
    }
  }
  return {};
}

void SetPreinstalledWebAppsMappingForTesting(  // IN-TEST
    absl::optional<base::flat_map<base::StringPiece, base::StringPiece>>
        preinstalled_web_apps_mapping_for_testing) {
  GetPreinstalledWebAppsMappingForTesting() =                // IN-TEST
      std::move(preinstalled_web_apps_mapping_for_testing);  // IN-TEST
}

}  // namespace apps_util
