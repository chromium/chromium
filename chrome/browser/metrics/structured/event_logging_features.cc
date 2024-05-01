// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/structured/event_logging_features.h"

#include "base/feature_list.h"
#include "components/metrics/structured/structured_metrics_features.h"

namespace metrics::structured {

constexpr base::FeatureParam<int> kOobeUploadCount{
    &kEnabledStructuredMetricsService, "oobe_upload_count", 10};

int GetOobeEventUploadCount() {
  return kOobeUploadCount.Get();
}

}  // namespace metrics::structured
