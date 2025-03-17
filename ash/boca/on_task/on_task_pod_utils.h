// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_BOCA_ON_TASK_ON_TASK_POD_UTILS_H_
#define ASH_BOCA_ON_TASK_ON_TASK_POD_UTILS_H_

#include "ash/ash_export.h"
#include "ui/views/widget/widget.h"

namespace ash::boca {

// Calculates the height of the `widget`'s frame header. Returns 0 if the frame
// header is not found or when the header is invisible.
ASH_EXPORT int GetFrameHeaderHeight(views::Widget* widget);

}  // namespace ash::boca

#endif  // ASH_BOCA_ON_TASK_ON_TASK_POD_UTILS_H_
