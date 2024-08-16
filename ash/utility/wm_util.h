// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_UTILITY_WM_UTIL_H_
#define ASH_UTILITY_WM_UTIL_H_

#include "ash/ash_export.h"
#include "ui/views/widget/widget.h"

namespace ash_util {

// Sets up `params` to place the widget in an ash shell window container on
// the display used for new windows. See ash/public/cpp/shell_window_ids.h for
// `container_id` values.
// TODO(jamescook): Extend to take a display_id.
ASH_EXPORT void SetupWidgetInitParamsForContainer(
    views::Widget::InitParams* params,
    int container_id);

// Sets up `params` to place the widget in an ash shell window container on
// the primary display. See ash/public/cpp/shell_window_ids.h for
// `container_id` values.
ASH_EXPORT void SetupWidgetInitParamsForContainerInPrimary(
    views::Widget::InitParams* params,
    int container_id);

// Returns the the SystemModalContainer id if a session is active, ortherwise
// returns the LockSystemModalContainer id so that the dialog appears above the
// lock screen.
ASH_EXPORT int GetSystemModalDialogContainerId();

}  // namespace ash_util

#endif  // ASH_UTILITY_WM_UTIL_H_
