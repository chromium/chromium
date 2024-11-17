// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"

#include "base/feature_list.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/pref_names.h"
#include "components/variations/seed_response.h"
#include "components/variations/service/google_groups_manager.h"
#include "components/variations/service/limited_entropy_synthetic_trial.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/synthetic_trials.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/upgrade_detector/build_state.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "base/check_is_test.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
#include "chromeos/startup/browser_params_proxy.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/enterprise_util.h"
#endif

ChromeVariationsServiceClient::ChromeVariationsServiceClient() = default;

ChromeVariationsServiceClient::~ChromeVariationsServiceClient() = default;

base::Version ChromeVariationsServiceClient::GetVersionForSimulation() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  const auto* build_state = g_browser_process->GetBuildState();
  if (build_state->installed_version().has_value())
    return *build_state->installed_version();
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

  // TODO(asvitkine): Get the version that will be used on restart instead of
  // the current version on Android, iOS and ChromeOS.
  return version_info::GetVersion();
}

scoped_refptr<network::SharedURLLoaderFactory>
ChromeVariationsServiceClient::GetURLLoaderFactory() {
  return g_browser_process->system_network_context_manager()
      ->GetSharedURLLoaderFactory();
}

network_time::NetworkTimeTracker*
ChromeVariationsServiceClient::GetNetworkTimeTracker() {
  return g_browser_process->network_time_tracker();
}

bool ChromeVariationsServiceClient::OverridesRestrictParameter(
    std::string* parameter) {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::CrosSettings::Get()->GetString(ash::kVariationsRestrictParameter,
                                      parameter);
  return true;
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  // The device settings is passed from Ash to Lacros via
  // crosapi::mojom::BrowserInitParams. However, crosapi is disabled for Lacros
  // browser_tests, there is no valid device settings in this situation.
  // Note: This code path is invoked when browser test starts the browser for
  // branded Lacros build, see crbug.com/1474764.
  if (!g_browser_process->browser_policy_connector()->GetDeviceSettings()) {
    CHECK_IS_TEST();  // IN-TEST
    CHECK(chromeos::BrowserParamsProxy::
              IsCrosapiDisabledForTesting());  // IN-TEST
    return false;
  }

  const std::optional<std::string>& policy_value =
      g_browser_process->browser_policy_connector()
          ->GetDeviceSettings()
          ->device_variations_restrict_parameter;
  if (!policy_value) {
    return false;
  }

  *parameter = *policy_value;
  return true;
#else
  return false;
#endif
}

base::FilePath ChromeVariationsServiceClient::GetVariationsSeedFileDir() {
  base::FilePath seed_file_dir;
  base::PathService::Get(chrome::DIR_USER_DATA, &seed_file_dir);
  return seed_file_dir;
}

std::unique_ptr<variations::SeedResponse>
ChromeVariationsServiceClient::TakeSeedFromNativeVariationsSeedStore() {
#if BUILDFLAG(IS_ANDROID)
  std::unique_ptr<variations::SeedResponse> seed =
      variations::android::GetVariationsFirstRunSeed();
  variations::android::ClearJavaFirstRunPrefs();
  return seed;
#else
  return nullptr;
#endif
}

bool ChromeVariationsServiceClient::IsEnterprise() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return base::IsEnterpriseDevice();
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return ash::InstallAttributes::Get()->IsEnterpriseManaged();
#else
  return false;
#endif
}

// Remove any profiles from variations prefs that no longer exist in the
// ProfileAttributesStorage source-of-truth.
void ChromeVariationsServiceClient::
    RemoveGoogleGroupsFromPrefsForDeletedProfiles(PrefService* local_state) {
  // Get the list of profiles in attribute storage.
  base::flat_set<std::string> profile_keys =
      ProfileAttributesStorage::GetAllProfilesKeys(local_state);

  // Get the current value of the local state dict.
  const base::Value::Dict& cached_profiles =
      local_state->GetDict(variations::prefs::kVariationsGoogleGroups);
  std::vector<std::string> variations_profiles_to_delete;
  for (std::pair<const std::string&, const base::Value&> profile :
       cached_profiles) {
    if (!profile_keys.contains(profile.first)) {
      variations_profiles_to_delete.push_back(profile.first);
    }
  }

  ScopedDictPrefUpdate variations_prefs_update(
      local_state, variations::prefs::kVariationsGoogleGroups);
  base::Value::Dict& variations_prefs_dict = variations_prefs_update.Get();
  for (const auto& profile : variations_profiles_to_delete) {
    variations_prefs_dict.Remove(profile);
  }
}

version_info::Channel ChromeVariationsServiceClient::GetChannel() {
  return chrome::GetChannel();
}
