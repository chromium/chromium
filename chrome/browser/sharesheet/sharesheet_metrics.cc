// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sharesheet/sharesheet_metrics.h"

#include "base/metrics/histogram_functions.h"

namespace {
// Should be comfortably larger than any max number of apps
// a user could have installed.
constexpr size_t kMaxAppCount = 1000;
}  // namespace

namespace sharesheet {

SharesheetMetrics::SharesheetMetrics() = default;

void SharesheetMetrics::RecordSharesheetActionMetrics(UserAction action) {
  base::UmaHistogramEnumeration("ChromeOS.Sharesheet.UserAction", action);
}

void SharesheetMetrics::RecordSharesheetAppCount(int app_count) {
  base::UmaHistogramExactLinear("ChromeOS.Sharesheet.AppCount", app_count,
                                kMaxAppCount);
}

}  // namespace sharesheet
