// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_METRICS_WEB_VITAL_METRICS_H_
#define CC_METRICS_WEB_VITAL_METRICS_H_

#include "base/time/time.h"
#include "cc/cc_export.h"

namespace cc {

// Web Vital metrics reported from blink to be displayed with cc's HUD display.
struct CC_EXPORT WebVitalMetrics {
  base::Optional<base::TimeDelta> largest_contentful_paint;
  base::Optional<base::TimeDelta> first_input_delay;

  WebVitalMetrics();
  WebVitalMetrics(const WebVitalMetrics& other);

  bool HasValue() const;
};

}  // namespace cc

#endif  // CC_METRICS_WEB_VITAL_METRICS_H_
