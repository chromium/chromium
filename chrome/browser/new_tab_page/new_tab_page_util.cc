// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/new_tab_page_util.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/webui/new_tab_page/new_tab_page_ui.h"
#include "components/optimization_guide/core/optimization_guide_logger.h"
#include "components/search/ntp_features.h"
#include "components/sync/service/sync_service.h"
#include "components/variations/service/variations_service.h"

namespace {

bool IsOsSupportedForCart() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC)
  return true;
#else
  return false;
#endif
}

bool IsOsSupportedForDrive() {
#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX)
  return true;
#else
  return false;
#endif
}

bool IsInUS() {
  return g_browser_process->GetApplicationLocale() == "en-US" &&
         GetVariationsServiceCountryCode(
             g_browser_process->variations_service()) == "us";
}

}  // namespace

// If feature is overridden manually or by finch, read the feature flag value.
// Otherwise filter by os, locale and country code.
bool IsCartModuleEnabled() {
  if (base::FeatureList::GetInstance()->IsFeatureOverridden(
          ntp_features::kNtpChromeCartModule.name)) {
    return base::FeatureList::IsEnabled(ntp_features::kNtpChromeCartModule);
  }
  return IsOsSupportedForCart() && IsInUS();
}

bool IsDriveModuleEnabled() {
  if (base::FeatureList::GetInstance()->IsFeatureOverridden(
          ntp_features::kNtpDriveModule.name)) {
    const bool force_enabled =
        base::FeatureList::IsEnabled(ntp_features::kNtpDriveModule);
    LogDriveModuleEnablement(force_enabled, force_enabled
                                                ? "feature flag forced on"
                                                : "feature flag forced off");
    return force_enabled;
  }
  const bool default_enabled = IsOsSupportedForDrive();
  LogDriveModuleEnablement(default_enabled, default_enabled
                                                ? "default feature flag value"
                                                : "default feature flag value");
  return default_enabled;
}

bool IsDriveModuleEnabledForProfile(Profile* profile) {
  if (!IsDriveModuleEnabled()) {
    return false;
  }

  // TODO(crbug.com/40837656): Explore not requiring sync for the drive
  // module to be enabled.
  auto* sync_service = SyncServiceFactory::GetForProfile(profile);
  if (!sync_service || !sync_service->IsSyncFeatureEnabled()) {
    LogDriveModuleEnablement(false, "no sync");
    return false;
  }

  if (!NewTabPageUI::IsManagedProfile(profile)) {
    LogDriveModuleEnablement(false, "account not managed");
    return false;
  }
  return true;
}

bool IsEnUSLocaleOnlyFeatureEnabled(const base::Feature& ntp_feature) {
  if (base::FeatureList::GetInstance()->IsFeatureOverridden(ntp_feature.name)) {
    return base::FeatureList::IsEnabled(ntp_feature);
  }
  return IsInUS();
}

std::string GetVariationsServiceCountryCode(
    variations::VariationsService* variations_service) {
  std::string country_code;
  if (!variations_service) {
    return country_code;
  }
  country_code = variations_service->GetStoredPermanentCountry();
  return country_code.empty() ? variations_service->GetLatestCountry()
                              : country_code;
}

void LogDriveModuleEnablement(bool enabled, const std::string& reason) {
  OPTIMIZATION_GUIDE_LOGGER(
      optimization_guide_common::mojom::LogSource::NTP_MODULE,
      OptimizationGuideLogger::GetInstance())
      << "Drive module " << (enabled ? "enabled: " : "disabled: ") << reason;
}
