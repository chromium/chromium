// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/policy_util.h"

#include <array>
#include <utility>

#include "base/containers/fixed_flat_map.h"
#include "base/no_destructor.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/web_app_id_constants.h"
#include "components/crx_file/id_util.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/containers/map_util.h"
#include "base/types/optional_util.h"
#include "chrome/browser/ash/file_manager/office_file_tasks.h"
#include "chrome/browser/ash/file_manager/virtual_tasks/id_constants.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace apps_util {

namespace {

#if BUILDFLAG(IS_CHROMEOS_ASH)

namespace fm_tasks = file_manager::file_tasks;

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
         {"face_ml", ash::SystemWebAppType::FACE_ML},
         {"vc_background", ash::SystemWebAppType::VC_BACKGROUND}});

constexpr ash::SystemWebAppType GetMaxSystemWebAppType() {
  return base::ranges::max(kSystemWebAppsMapping, base::ranges::less{},
                           &decltype(kSystemWebAppsMapping)::value_type::second)
      .second;
}

static_assert(GetMaxSystemWebAppType() == ash::SystemWebAppType::kMaxValue,
              "Not all SWA types are listed in |system_web_apps_mapping|.");

// These virtual task identifiers are supposed to be a subset of tasks listed in
// chrome/browser/ash/file_manager/virtual_file_tasks.cc
constexpr auto kVirtualFileTasksMapping =
    base::MakeFixedFlatMap<base::StringPiece, base::StringPiece>(
        {{"install-isolated-web-app", fm_tasks::kActionIdInstallIsolatedWebApp},
         {"microsoft-office", fm_tasks::kActionIdOpenInOffice}});

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

bool IsChromeAppPolicyId(base::StringPiece policy_id) {
  return crx_file::id_util::IdIsValid(policy_id);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsArcAppPolicyId(base::StringPiece policy_id) {
  return base::Contains(policy_id, '.') && !IsWebAppPolicyId(policy_id);
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

#if BUILDFLAG(IS_CHROMEOS_ASH)
bool IsFileManagerVirtualTaskPolicyId(base::StringPiece policy_id) {
  return GetVirtualTaskIdFromPolicyId(policy_id).has_value();
}

absl::optional<base::StringPiece> GetVirtualTaskIdFromPolicyId(
    base::StringPiece policy_id) {
  if (!base::StartsWith(policy_id, kVirtualTaskPrefix)) {
    return absl::nullopt;
  }
  static constexpr size_t kOffset =
      std::char_traits<char>::length(kVirtualTaskPrefix);
  return base::OptionalFromPtr(
      base::FindOrNull(kVirtualFileTasksMapping, policy_id.substr(kOffset)));
}
#endif

std::string TransformRawPolicyId(const std::string& raw_policy_id) {
  if (const GURL raw_policy_id_gurl{raw_policy_id};
      raw_policy_id_gurl.is_valid()) {
    return raw_policy_id_gurl.spec();
  }

  return raw_policy_id;
}

std::vector<std::string> GetAppIdsFromPolicyId(Profile* profile,
                                               const std::string& policy_id) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return {};
  }
  std::vector<std::string> app_ids;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForEachApp([&policy_id, &app_ids](const apps::AppUpdate& update) {
        if (IsInstalled(update.Readiness()) &&
            base::Contains(update.PolicyIds(), policy_id)) {
          app_ids.push_back(update.AppId());
        }
      });
  return app_ids;
}

absl::optional<std::vector<std::string>> GetPolicyIdsFromAppId(
    Profile* profile,
    const std::string& app_id) {
  if (!apps::AppServiceProxyFactory::IsAppServiceAvailableForProfile(profile)) {
    return absl::nullopt;
  }
  absl::optional<std::vector<std::string>> policy_ids;
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [&policy_ids](const apps::AppUpdate& update) {
        policy_ids = update.PolicyIds();
      });
  return policy_ids;
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
