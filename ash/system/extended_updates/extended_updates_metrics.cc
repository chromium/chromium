// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/extended_updates/extended_updates_metrics.h"

#include <string>

#include "base/metrics/histogram_functions.h"

namespace ash {

void RecordExtendedUpdatesDialogEvent(ExtendedUpdatesDialogEvent event) {
  base::UmaHistogramEnumeration(kExtendedUpdatesDialogEventMetric, event);
}

void RecordExtendedUpdatesEntryPointEvent(
    ExtendedUpdatesEntryPointEvent event) {
  base::UmaHistogramEnumeration(kExtendedUpdatesEntryPointEventMetric, event);
}

}  // namespace ash
