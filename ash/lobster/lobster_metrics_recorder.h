// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_LOBSTER_LOBSTER_METRICS_RECORDER_H_
#define ASH_LOBSTER_LOBSTER_METRICS_RECORDER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/lobster/lobster_metrics_state_enums.h"

namespace ash {

void ASH_EXPORT RecordLobsterState(LobsterMetricState state);

}  // namespace ash

#endif  // ASH_LOBSTER_LOBSTER_METRICS_RECORDER_H_
