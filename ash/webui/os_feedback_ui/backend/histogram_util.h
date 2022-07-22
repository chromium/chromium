// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HISTOGRAM_UTIL_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HISTOGRAM_UTIL_H_

namespace base {
class TimeDelta;
}  // namespace base

namespace ash::os_feedback_ui::metrics {

constexpr char kFeedbackAppOpenDuration[] = "Feedback.ChromeOSApp.OpenDuration";

void EmitFeedbackAppOpenDuration(const base::TimeDelta& time_elapsed);

}  // namespace ash::os_feedback_ui::metrics

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HISTOGRAM_UTIL_H_