// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/chrome_variations_service_client.h"

#include "base/bind.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/net/system_network_context_manager.h"
#include "chrome/common/channel_info.h"
#include "components/variations/service/variations_service_client.h"
#include "components/version_info/version_info.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"

#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/upgrade_detector/upgrade_detector_impl.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/settings/cros_settings.h"
#include "chromeos/components/chromebox_for_meetings/buildflags/buildflags.h"
#endif

#if defined(OS_WIN) || defined(OS_MAC)
#include "base/enterprise_util.h"
#elif BUILDFLAG(IS_CHROMEOS_ASH)
#include "chromeos/tpm/install_attributes.h"
#endif

namespace {

// Gets the version number to use for variations seed simulation. Must be called
// on a thread where IO is allowed.
base::Version GetVersionForSimulation() {
#if !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)
  const base::Version installed_version =
      UpgradeDetectorImpl::GetCurrentlyInstalledVersion();
  if (installed_version.IsValid())
    return installed_version;
#endif  // !defined(OS_ANDROID) && !BUILDFLAG(IS_CHROMEOS_ASH)

  // TODO(asvitkine): Get the version that will be used on restart instead of
  // the current version on Android, iOS and ChromeOS.
  return version_info::GetVersion();
}

}  // namespace

ChromeVariationsServiceClient::ChromeVariationsServiceClient() {}

ChromeVariationsServiceClient::~ChromeVariationsServiceClient() {}

ChromeVariationsServiceClient::VersionCallback
ChromeVariationsServiceClient::GetVersionForSimulationCallback() {
  return base::BindOnce(&GetVersionForSimulation);
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
  ash::CrosSettings::Get()->GetString(chromeos::kVariationsRestrictParameter,
                                      parameter);
  return true;
#else
  return false;
#endif
}

variations::Study::FormFactor
ChromeVariationsServiceClient::GetCurrentFormFactor() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
#if BUILDFLAG(PLATFORM_CFM)
  return variations::Study::MEET_DEVICE;
#endif  // BUILDFLAG(PLATFORM_CFM)
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  return variations::VariationsServiceClient::GetCurrentFormFactor();
}

bool ChromeVariationsServiceClient::IsEnterprise() {
#if defined(OS_WIN) || defined(OS_MAC)
  return base::IsMachineExternallyManaged();
#elif BUILDFLAG(IS_CHROMEOS_ASH)
  return chromeos::InstallAttributes::Get()->IsEnterpriseManaged();
#else
  return false;
#endif
}

version_info::Channel ChromeVariationsServiceClient::GetChannel() {
  return chrome::GetChannel();
}
