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
}

void WallpaperSearchService::PrepareToEnableOnRestart() {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(b/314795114): Enabling these flags on ChromeOS Ash might cause the
  // browser to crash.
  std::unique_ptr<flags_ui::FlagsStorage> flags_storage =
      std::make_unique<flags_ui::PrefServiceFlagsStorage>(
          g_browser_process->local_state());
  EnableWallpaperSearchFeatures(flags_storage.get());
#endif  // !BUILDFLAG(IS_CHROMEOS_ASH)
}
