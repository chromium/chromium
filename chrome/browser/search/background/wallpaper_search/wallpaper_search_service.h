// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_SERVICE_H_
#define CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_SERVICE_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/flags_ui/pref_service_flags_storage.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/core/model_execution/settings_enabled_observer.h"

class Profile;

// A service that enables GM3 features when Wallpaper Search is enabled.
class WallpaperSearchService
    : public KeyedService,
      public optimization_guide::SettingsEnabledObserver {
 public:
  explicit WallpaperSearchService(Profile* profile);

  WallpaperSearchService(const WallpaperSearchService&) = delete;
  WallpaperSearchService& operator=(const WallpaperSearchService&) = delete;

  ~WallpaperSearchService() override;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SkipChromeOSDeviceCheckForTesting(bool should_skip_check) {
    skip_chrome_os_device_check_for_testing_ = should_skip_check;
  }
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

 private:
  // KeyedService implementation:
  void Shutdown() override;

  void EnableWallpaperSearchFeatures(flags_ui::FlagsStorage* flags_storage);
#if BUILDFLAG(IS_CHROMEOS_ASH)
  void EnableWallpaperSearchFeaturesForChromeAsh(bool is_owner);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  // optimization_guide::SettingsEnabledObserver implementation;
  void PrepareToEnableOnRestart() override;

  raw_ptr<Profile> profile_;
  raw_ptr<OptimizationGuideKeyedService> optimization_guide_keyed_service_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  bool skip_chrome_os_device_check_for_testing_ = false;
#endif
  base::WeakPtrFactory<WallpaperSearchService> weak_factory_{this};
};

#endif  // CHROME_BROWSER_SEARCH_BACKGROUND_WALLPAPER_SEARCH_WALLPAPER_SEARCH_SERVICE_H_
