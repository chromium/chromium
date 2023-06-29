// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/desktop_platform_features_metrics_provider.h"

#include <vector>

#include "base/feature_list.h"
#include "base/metrics/histogram_macros.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/reading_list/reading_list_model_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "components/reading_list/core/reading_list_model.h"
#include "ui/native_theme/native_theme.h"

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
  ui::NativeTheme* theme = ui::NativeTheme::GetInstanceForNativeUi();
  DarkModeStatus status = DarkModeStatus::kUnavailable;
  if (ui::NativeTheme::SystemDarkModeSupported()) {
    status = theme->ShouldUseDarkColors() ? DarkModeStatus::kDark
                                          : DarkModeStatus::kLight;
  }
  UMA_HISTOGRAM_ENUMERATION("Browser.DarkModeStatus", status);

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
}
