// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ambient/metrics/ambient_consumer_session_metrics_delegate.h"

#include <utility>

#include "ash/ambient/metrics/ambient_metrics.h"
#include "base/check.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "ui/display/screen.h"

namespace ash {

AmbientConsumerSessionMetricsDelegate::AmbientConsumerSessionMetricsDelegate(
    AmbientUiSettings ui_settings)
    : ui_settings_(std::move(ui_settings)) {}

AmbientConsumerSessionMetricsDelegate::
    ~AmbientConsumerSessionMetricsDelegate() = default;

void AmbientConsumerSessionMetricsDelegate::RecordInitStatus(bool success) {}

void AmbientConsumerSessionMetricsDelegate::RecordStartupTime(
    base::TimeDelta startup_time) {
  ambient::RecordAmbientModeStartupTime(startup_time, ui_settings_);
}

void AmbientConsumerSessionMetricsDelegate::RecordEngagementTime(
    base::TimeDelta engagement_time) {
  ambient::RecordAmbientModeTimeElapsed(engagement_time,
                                        display::Screen::Get()->InTabletMode());
}

}  // namespace ash
