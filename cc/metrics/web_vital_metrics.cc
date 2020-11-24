// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/metrics/web_vital_metrics.h"

namespace cc {

constexpr WebVitalMetrics::MetricsInfo WebVitalMetrics::lcp_info;
constexpr WebVitalMetrics::MetricsInfo WebVitalMetrics::fid_info;
constexpr WebVitalMetrics::MetricsInfo WebVitalMetrics::cls_info;

WebVitalMetrics::WebVitalMetrics() = default;

WebVitalMetrics::WebVitalMetrics(const WebVitalMetrics& other) = default;

bool WebVitalMetrics::HasValue() const {
  if (largest_contentful_paint.has_value())
    return true;

  if (first_input_delay.has_value())
    return true;

  if (layout_shift > 0.f)
    return true;

  return false;
}

}  // namespace cc
