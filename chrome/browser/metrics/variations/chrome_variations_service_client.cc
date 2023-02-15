// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "build/config/chromebox_for_meetings/buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/channel_info.h"
#include "components/variations/seed_response.h"
#include "components/variations/service/variations_service_client.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/upgrade_detector/build_state.h"
#endif

#if BUILDFLAG(IS_ANDROID)
#include "components/variations/android/variations_seed_bridge.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chromeos/crosapi/mojom/device_settings_service.mojom.h"
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
  const absl::optional<std::string>& policy_value =
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

variations::Study::FormFactor
ChromeVariationsServiceClient::GetCurrentFormFactor() {
#if BUILDFLAG(PLATFORM_CFM)
  return variations::Study::MEET_DEVICE;
#else
  return variations::VariationsServiceClient::GetCurrentFormFactor();
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

version_info::Channel ChromeVariationsServiceClient::GetChannel() {
  return chrome::GetChannel();
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
