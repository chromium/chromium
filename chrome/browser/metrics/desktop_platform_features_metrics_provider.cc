// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_platform_features_metrics_provider.h"

#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/browser_window_features.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface_iterator.h"
#include "chrome/browser/ui/location_bar/location_bar.h"
#include "components/reading_list/core/reading_list_model.h"
#include "ui/base/base_window.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size.h"
#include "ui/native_theme/native_theme.h"
#include "ui/native_theme/os_settings_provider.h"

#if BUILDFLAG(IS_WIN)
#include "chrome/browser/win/isolated_browser_support.h"
#endif  // BUILDFLAG(IS_WIN)

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DarkModeStatus {
  kUnavailable = 0,
  kLight = 1,
  kDark = 2,
  kMaxValue = kDark,
};

}  // namespace

DesktopPlatformFeaturesMetricsProvider::
    DesktopPlatformFeaturesMetricsProvider() = default;
DesktopPlatformFeaturesMetricsProvider::
    ~DesktopPlatformFeaturesMetricsProvider() = default;

void DesktopPlatformFeaturesMetricsProvider::ProvideCurrentSessionData(
    metrics::ChromeUserMetricsExtension* uma_proto) {
  DarkModeStatus status = DarkModeStatus::kUnavailable;
  if (ui::OsSettingsProvider::Get().DarkColorSchemeAvailable()) {
    status =
        (ui::NativeTheme::GetInstanceForNativeUi()->preferred_color_scheme() ==
         ui::NativeTheme::PreferredColorScheme::kDark)
            ? DarkModeStatus::kDark
            : DarkModeStatus::kLight;
  }
  base::UmaHistogramEnumeration("Browser.DarkModeStatus", status);

  // Record how many items are in the reading list.
  std::vector<Profile*> profiles =
      g_browser_process->profile_manager()->GetLoadedProfiles();
  for (Profile* profile : profiles) {
    ReadingListModel* model =
        ReadingListModelFactory::GetForBrowserContext(profile);
    if (model) {
      model->RecordCountMetricsOnUMAUpload();
    }
  }

  ForEachCurrentBrowserWindowInterfaceOrderedByActivation(
      [](BrowserWindowInterface* browser) {
        const ui::BaseWindow* window = browser->GetWindow();
        if (!window || !window->IsVisible() || window->IsMinimized()) {
          return true;
        }

        const gfx::Size window_size = window->GetBounds().size();
        if (window_size.IsEmpty()) {
          return true;
        }

        // A 4K screen is 4096 pixels wide. Doubling this and rounding up to
        // 10000 should give a reasonable upper bound on DIPs.
        base::UmaHistogramCounts100000("Tabs.WindowWidth2",
                                       window_size.width());

        // Record the width of the omnibox.
        LocationBar* location_bar = browser->GetFeatures().location_bar();
        if (location_bar && location_bar->IsVisible()) {
          base::UmaHistogramCounts100000("Omnibox.Width",
                                         location_bar->Bounds().width());

          base::UmaHistogramPercentage(
              "Omnibox.WidthRatioToWindow",
              location_bar->Bounds().width() * 100 / window_size.width());
        }
        return true;
      });

#if BUILDFLAG(IS_WIN)
  base::UmaHistogramBoolean("Windows.RunningIsolated",
                            chrome::IsRunningIsolated());
#endif  // BUILDFLAG(IS_WIN)
}
