// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_WM_FEATURE_METRICS_RECORDER_H_
#define ASH_METRICS_WM_FEATURE_METRICS_RECORDER_H_

#include <string>

#include "ash/ash_export.h"

namespace ash {

class ASH_EXPORT WMFeatureMetricsRecorder {
 public:
  // Different types of WM features.
  enum class WMFeatureType {
    kWindowLayoutState,
  };

  // Different window size range. See `kWidthRange` and `kHeightRange` in the cc
  // file.
  // Note this should be kept in sync with `WindowSizeRange` enum in
  //  tools/metrics/histograms/enums.xml.
  enum class WindowSizeRange : int {
    kXSWidthXSHeight = 0,  // (<=800, <=600)
    kXSWidthSHeight = 1,   // (<=800, 600-728)
    kXSWidthMHeight = 2,   // (<=800, 728-900)
    kXSWidthLHeight = 3,   // (<=800, >900)

    kSWidthXSHeight = 4,  // (800-1024, <=600)
    kSWidthSHeight = 5,   // (800-1024, 600-728)
    kSWidthMHeight = 6,   // (800-1024, 728-900)
    kSWidthLHeight = 7,   // (800-1024, >900)

    kMWidthXSHeight = 8,  // (1024-1400, <=600)
    kMWidthSHeight = 9,   // (1024-1400, 600-728)
    kMWidthMHeight = 10,  // (1024-1400, 728-900)
    kMWidthLHeight = 11,  // (1024-1400, >900)

    kLWidthXSHeight = 12,  // (>1400, <=600)
    kLWidthSHeight = 13,   // (>1400, 600-728)
    kLWidthMHeight = 14,   // (>1400, 728-900)
    kLWidthLHeight = 15,   // (>1400, >900)

    kMaxValue = kLWidthLHeight,
  };

  WMFeatureMetricsRecorder();
  WMFeatureMetricsRecorder(WMFeatureMetricsRecorder&) = delete;
  WMFeatureMetricsRecorder& operator=(WMFeatureMetricsRecorder&) = delete;
  ~WMFeatureMetricsRecorder();

  // Gets the feature metrics prefix for `wm_feature_type`.
  static std::string GetFeatureMetricsPrefix(
      const WMFeatureType& wm_feature_type);

  // Called by `UserMetricsRecorder` to periodically report WM feature related
  // metrics.
  void RecordPeriodicalWMMetrics();
};

}  // namespace ash

#endif  // ASH_METRICS_WM_FEATURE_METRICS_RECORDER_H_
