// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/floating_workspace/floating_workspace_metrics_util.h"
#include "base/metrics/histogram_functions.h"

namespace ash::floating_workspace_metrics_util {

void RecordFloatingWorkspaceV1InitializedHistogram() {
  base::UmaHistogramBoolean(kFloatingWorkspaceV1Initialized, true);
}

void RecordFloatingWorkspaceV1RestoredSessionType(
    RestoredBrowserSessionType type) {
  base::UmaHistogramEnumeration(kFloatingWorkspaceV1RestoredSessionType, type);
}

}  // namespace ash::floating_workspace_metrics_util
