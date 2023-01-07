// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/overview_metrics.h"

#include "base/metrics/histogram_macros.h"

namespace ash {

constexpr char kOverviewStartActionHistogram[] = "Ash.Overview.StartAction";
constexpr char kOverviewEndActionHistogram[] = "Ash.Overview.EndAction";

void RecordOverviewStartAction(OverviewStartAction type) {
  UMA_HISTOGRAM_ENUMERATION(kOverviewStartActionHistogram, type);
}

void RecordOverviewEndAction(OverviewEndAction type) {
  UMA_HISTOGRAM_ENUMERATION(kOverviewEndActionHistogram, type);
}

}  // namespace ash
