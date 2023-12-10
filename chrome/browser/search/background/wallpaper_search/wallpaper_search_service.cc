// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/background/wallpaper_search/wallpaper_search_service.h"
#include "chrome/browser/about_flags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/flag_descriptions.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/flags/flags_ui_handler.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "base/system/sys_info.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash.h"
#include "chrome/browser/ash/ownership/owner_settings_service_ash_factory.h"
#include "chrome/browser/ash/settings/about_flags.h"
#endif

WallpaperSearchService::WallpaperSearchService(Profile* profile)
    : SettingsEnabledObserver(optimization_guide::proto::ModelExecutionFeature::
                                  MODEL_EXECUTION_FEATURE_WALLPAPER_SEARCH),
      profile_(profile),
      optimization_guide_keyed_service_(
          OptimizationGuideKeyedServiceFactory::GetForProfile(profile_)) {
  if (optimization_guide_keyed_service_) {
    optimization_guide_keyed_service_->AddModelExecutionSettingsEnabledObserver(
        this);
  }
}
WallpaperSearchService::~WallpaperSearchService() = default;

void WallpaperSearchService::Shutdown() {
  if (optimization_guide_keyed_service_) {
    optimization_guide_keyed_service_
        ->RemoveModelExecutionSettingsEnabledObserver(this);
    optimization_guide_keyed_service_ = nullptr;
  }
}

void WallpaperSearchService::EnableWallpaperSearchFeatures(
    flags_ui::FlagsStorage* flags_storage) {
  about_flags::SetFeatureEntryEnabled(
      flags_storage,
      std::string(flag_descriptions::kChromeRefresh2023Id) +
          flags_ui::kMultiSeparatorChar +
          /*enable_feature_index=*/"1",
      true);
  about_flags::SetFeatureEntryEnabled(
      flags_storage,
      std::string(flag_descriptions::kChromeWebuiRefresh2023Id) +
          flags_ui::kMultiSeparatorChar +
          /*enable_feature_index=*/"1",
      true);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  ash::about_flags::FeatureFlagsUpdate(
      *flags_storage, profile_->GetOriginalProfile()->GetPrefs())
      .UpdateSessionManager();
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
void WallpaperSearchService::EnableWallpaperSearchFeaturesForChromeAsh(
    bool is_owner) {
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage;
  Profile* original_profile = profile_->GetOriginalProfile();
  if (is_owner) {
    flags_storage = std::make_unique<ash::about_flags::OwnerFlagsStorage>(
        original_profile->GetPrefs(),
        ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
            original_profile));
  } else {
    flags_storage = std::make_unique<flags_ui::PrefServiceFlagsStorage>(
        original_profile->GetPrefs());
  }
  EnableWallpaperSearchFeatures(flags_storage.get());
}
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

void WallpaperSearchService::PrepareToEnableOnRestart() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Bypass possible incognito profile same as chrome://flags does.
  Profile* original_profile = profile_->GetOriginalProfile();
  // Chrome OS builds sometimes run on non-Chrome OS environments.
  if ((base::SysInfo::IsRunningOnChromeOS() ||
       skip_chrome_os_device_check_for_testing_) &&
      ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(
          original_profile)) {
    // Ash-chrome uses a different FlagsStorage if the user is the owner.
    ash::OwnerSettingsServiceAshFactory::GetForBrowserContext(original_profile)
        ->IsOwnerAsync(base::BindOnce(
            &WallpaperSearchService::EnableWallpaperSearchFeaturesForChromeAsh,
            weak_factory_.GetWeakPtr()));
    return;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage =
      std::make_unique<flags_ui::PrefServiceFlagsStorage>(
          g_browser_process->local_state());
  EnableWallpaperSearchFeatures(flags_storage.get());
}
