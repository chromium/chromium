// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ash/webui/os_feedback_ui/backend/histogram_util.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"

namespace ash::os_feedback_ui::metrics {

void EmitFeedbackAppOpenDuration(const base::TimeDelta& time_elapsed) {
  base::UmaHistogramLongTimes100(kFeedbackAppOpenDuration, time_elapsed);
}

}  // namespace ash::os_feedback_ui::metrics
