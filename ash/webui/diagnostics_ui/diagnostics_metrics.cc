// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/diagnostics_metrics.h"

#include "ash/constants/ash_features.h"

namespace ash {
namespace diagnostics {
namespace metrics {

bool DiagnosticsMetrics::IsEligible() const {
  return features::IsDiagnosticsAppEnabled();
}

bool DiagnosticsMetrics::IsEnabled() const {
  return features::IsDiagnosticsAppEnabled();
}
}  // namespace metrics
}  // namespace diagnostics
}  // namespace ash
