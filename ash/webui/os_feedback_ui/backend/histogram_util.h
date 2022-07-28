// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HISTOGRAM_UTIL_H_
#define ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HISTOGRAM_UTIL_H_

#include "ash/webui/os_feedback_ui/mojom/os_feedback_ui.mojom.h"

namespace base {
class TimeDelta;
}  // namespace base

namespace ash::os_feedback_ui::metrics {

constexpr char kFeedbackAppOpenDuration[] = "Feedback.ChromeOSApp.OpenDuration";
constexpr char kFeedbackAppPostSubmitAction[] =
    "Feedback.ChromeOSApp.PostSubmitAction";
constexpr char kFeedbackAppIncludedScreenshot[] =
    "Feedback.ChromeOSApp.IncludedScreenshot";

void EmitFeedbackAppOpenDuration(const base::TimeDelta& time_elapsed);

void EmitFeedbackAppPostSubmitAction(mojom::FeedbackAppPostSubmitAction action);

void EmitFeedbackAppIncludedScreenshot(bool included_screenshot);

}  // namespace ash::os_feedback_ui::metrics

#endif  // ASH_WEBUI_OS_FEEDBACK_UI_BACKEND_HISTOGRAM_UTIL_H_