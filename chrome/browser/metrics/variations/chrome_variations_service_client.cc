// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"

#include "base/feature_list.h"
#include "base/path_service.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/metrics/variations/google_groups_manager_factory.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/browser/profiles/profile_attributes_storage.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/pref_names.h"
#include "components/enterprise/browser/groups/groups_prefs.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/variations/pref_names.h"
#include "components/variations/seed_response.h"
#include "components/variations/service/google_groups_manager.h"
#include "components/variations/service/variations_service_client.h"
#include "components/variations/service/variations_service_utils.h"
#include "components/variations/synthetic_trials.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
#include "chrome/browser/upgrade_detector/build_state.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#endif

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
#include "base/enterprise_util.h"
#endif

ChromeVariationsServiceClient::ChromeVariationsServiceClient() = default;

ChromeVariationsServiceClient::~ChromeVariationsServiceClient() = default;

base::Version ChromeVariationsServiceClient::GetVersionForSimulation() {
#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)
  const auto* build_state = g_browser_process->GetBuildState();
  if (build_state->installed_version().has_value())
    return *build_state->installed_version();
#endif  // !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS)

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
#if BUILDFLAG(IS_CHROMEOS)
  ash::CrosSettings::Get()->GetString(ash::kVariationsRestrictParameter,
                                      parameter);
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
#elif BUILDFLAG(IS_CHROMEOS)
  return ash::InstallAttributes::Get()->IsEnterpriseManaged();
#else
  return false;
#endif
}

// Remove any profiles from variations prefs that no longer exist in the
// ProfileAttributesStorage source-of-truth.
void ChromeVariationsServiceClient::
    RemoveGoogleGroupsFromPrefsForDeletedProfiles(PrefService* local_state) {
  variations::RemovePrefsForDeletedProfiles(
      local_state, variations::prefs::kVariationsGoogleGroups,
      ProfileAttributesStorage::GetAllProfilesKeys(local_state));
}

void ChromeVariationsServiceClient::
    RemoveEnterpriseGroupsFromPrefsForDeletedProfiles(
        PrefService* local_state) {
  variations::RemovePrefsForDeletedProfiles(
      local_state, enterprise_groups::kEnterpriseGroupsProfilePref,
      ProfileAttributesStorage::GetAllProfilesKeys(local_state));
}

version_info::Channel ChromeVariationsServiceClient::GetChannel() {
  return chrome::GetChannel();
}
