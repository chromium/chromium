// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/lobster/lobster_metrics_recorder.h"

#include "ash/public/cpp/lobster/lobster_metrics_state_enums.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

void RecordLobsterState(LobsterMetricState state) {
  base::UmaHistogramEnumeration("Ash.Lobster.State", state);
}

}  // namespace ash
