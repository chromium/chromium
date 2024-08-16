// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ash/metrics/wm_feature_metrics_recorder.h"

#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chromeos/ui/base/app_types.h"
#include "chromeos/ui/base/window_properties.h"

namespace ash {

namespace {

// The Metrics prefix for WM features.
constexpr char kWMFeatureMetricPrefix[] = "Ash.Wm.";

// Pre-defined Window size ranges.
constexpr int kWidthRange[] = {0, 800, 1024, 1400};
constexpr int kHeightRange[] = {0, 600, 728, 900};

WMFeatureMetricsRecorder::WindowSizeRange GetWindowSizeRange(
    const gfx::Size& window_size) {
  const int width = window_size.width();
  const int height = window_size.height();

  int width_index = 0;
  for (size_t i = 0; i < sizeof(kWidthRange) / sizeof(int) - 1; ++i) {
    if (width > kWidthRange[i] && width <= kWidthRange[i + 1]) {
      width_index = i;
      break;
    }
    if (width > kWidthRange[i + 1]) {
      width_index = i + 1;
    }
  }

  int height_index = 0;
  for (size_t j = 0; j < sizeof(kHeightRange) / sizeof(int) - 1; ++j) {
    if (height > kHeightRange[j] && height <= kHeightRange[j + 1]) {
      height_index = j;
      break;
    }
    if (height > kHeightRange[j + 1]) {
      height_index = j + 1;
    }
  }

  return static_cast<WMFeatureMetricsRecorder::WindowSizeRange>(
      width_index * (sizeof(kHeightRange) / sizeof(int)) + height_index);
}

// Records the window state and layout related metrics for all windows and the
// current active window.
void RecordWindowLayoutAndStatePeriodically() {
  // Get all windows and their placement configuration.
  auto windows =
      Shell::Get()->mru_window_tracker()->BuildMruWindowList(kAllDesks);

  const std::string metrics_prefix =
      WMFeatureMetricsRecorder::GetFeatureMetricsPrefix(
          WMFeatureMetricsRecorder::WMFeatureType::kWindowLayoutState);
  // Report the number of the opened windows.
  base::UmaHistogramCounts100(metrics_prefix + "WindowNumbers", windows.size());

  aura::Window* active_window = window_util::GetActiveWindow();
  for (aura::Window* window : windows) {
    const bool is_active_window = window == active_window;
    std::vector<std::string> metrics_suffixes;

    // Report the window state types for all opened windows and the active
    // window.
    auto state_type = WindowState::Get(window)->GetStateType();
    metrics_suffixes.push_back("AllWindowStates");
    if (is_active_window) {
      metrics_suffixes.push_back("ActiveWindowState");
    }
    for (const std::string& metrics_suffix : metrics_suffixes) {
      base::UmaHistogramEnumeration(metrics_prefix + metrics_suffix,
                                    state_type);
    }

    // Report the app types for all opened windows and the active window.
    metrics_suffixes.clear();
    metrics_suffixes.push_back("AllAppTypes");
    if (is_active_window) {
      metrics_suffixes.push_back("ActiveWindowAppType");
    }
    for (const std::string& metrics_suffix : metrics_suffixes) {
      base::UmaHistogramEnumeration(metrics_prefix + metrics_suffix,
                                    window->GetProperty(chromeos::kAppTypeKey));
    }

    // Report the sizes for all windows and the active window.
    metrics_suffixes.clear();
    metrics_suffixes.push_back("AllWindowSizes");
    if (chromeos::IsNormalWindowStateType(state_type)) {
      metrics_suffixes.push_back("FreeformedWindowSizes");
    }
    if (is_active_window) {
      metrics_suffixes.push_back("ActiveWindowSize");
    }
    const WMFeatureMetricsRecorder::WindowSizeRange size_range =
        GetWindowSizeRange(window->bounds().size());
    for (const std::string& metrics_suffix : metrics_suffixes) {
      base::UmaHistogramEnumeration(metrics_prefix + metrics_suffix,
                                    size_range);
    }
  }
}

}  // namespace

WMFeatureMetricsRecorder::WMFeatureMetricsRecorder() = default;

WMFeatureMetricsRecorder::~WMFeatureMetricsRecorder() = default;

// static
std::string WMFeatureMetricsRecorder::GetFeatureMetricsPrefix(
    const WMFeatureType& wm_feature_type) {
  switch (wm_feature_type) {
    case WMFeatureType::kWindowLayoutState:
      return base::StrCat({kWMFeatureMetricPrefix, "WindowLayoutState."});
  }
}

void WMFeatureMetricsRecorder::RecordPeriodicalWMMetrics() {
  RecordWindowLayoutAndStatePeriodically();
}

}  // namespace ash
