// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/night_light/night_light_metrics_recorder.h"

#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/cpp/session/session_observer.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/system/night_light/night_light_controller_impl.h"
#include "base/metrics/histogram_functions.h"

namespace ash {

NightLightMetricsRecorder::NightLightMetricsRecorder() {
  Shell::Get()->session_controller()->AddObserver(this);
}

NightLightMetricsRecorder::~NightLightMetricsRecorder() {
  Shell::Get()->session_controller()->RemoveObserver(this);
}

void NightLightMetricsRecorder::OnFirstSessionStarted() {
  NightLightControllerImpl* night_light_controller =
      Shell::Get()->night_light_controller();

  // Record the schedule type.
  const ScheduleType schedule_type = night_light_controller->GetScheduleType();
  base::UmaHistogramEnumeration("Ash.NightLight.ScheduleType.Initial",
                                schedule_type);

  // Record the color temperature.
  const bool should_record_temperature =
      night_light_controller->IsNightLightEnabled() ||
      schedule_type != ScheduleType::kNone;
  if (should_record_temperature) {
    const int temperature_as_int =
        std::round(night_light_controller->GetColorTemperature() * 100);
    base::UmaHistogramPercentage("Ash.NightLight.Temperature.Initial",
                                 temperature_as_int);
  }
}

}  // namespace ash
