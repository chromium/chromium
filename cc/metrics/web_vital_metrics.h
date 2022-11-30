// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_WEB_VITAL_METRICS_H_
#define CC_METRICS_WEB_VITAL_METRICS_H_

#include <string>

#include "base/time/time.h"
#include "cc/cc_export.h"

namespace cc {

// Web Vital metrics reported from blink to be displayed with cc's HUD display.
struct CC_EXPORT WebVitalMetrics {
  bool has_lcp = false;
  base::TimeDelta largest_contentful_paint;
  bool has_fid = false;
  base::TimeDelta first_input_delay;
  bool has_cls = false;
  double layout_shift = 0.f;

  WebVitalMetrics() = default;
  WebVitalMetrics(const WebVitalMetrics& other) = default;

  bool HasValue() const { return has_lcp || has_fid || has_cls; }

  struct MetricsInfo {
    double green_threshold;
    double yellow_threshold;
    enum Unit { kSecond, kMillisecond, kScore };
    Unit unit;
    std::string UnitToString() const {
      switch (unit) {
        case Unit::kSecond:
          return " s";
        case Unit::kMillisecond:
          return " ms";
        case Unit::kScore:
        default:
          return "";
      }
    }
  };
  static constexpr MetricsInfo lcp_info = {
      2.5f, 4.f, WebVitalMetrics::MetricsInfo::Unit::kSecond};
  static constexpr MetricsInfo fid_info = {
      100, 300, WebVitalMetrics::MetricsInfo::Unit::kMillisecond};
  static constexpr MetricsInfo cls_info = {
      0.1f, 0.25f, WebVitalMetrics::MetricsInfo::Unit::kScore};
};

}  // namespace cc

#endif  // CC_METRICS_WEB_VITAL_METRICS_H_
