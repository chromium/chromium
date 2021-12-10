// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/desks/templates/desks_templates_metrics_util.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

void RecordLoadTemplateHistogram() {
  base::UmaHistogramBoolean(kLoadTemplateGridHistogramName, true);
}

void RecordDeleteTemplateHistogram() {
  base::UmaHistogramBoolean(kDeleteTemplateHistogramName, true);
}

void RecordLaunchTemplateHistogram() {
  base::UmaHistogramBoolean(kLaunchTemplateHistogramName, true);
}

void RecordNewTemplateHistogram() {
  base::UmaHistogramBoolean(kNewTemplateHistogramName, true);
}

}  // namespace ash