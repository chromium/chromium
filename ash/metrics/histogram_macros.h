// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_HISTOGRAM_MACROS_H_
#define ASH_METRICS_HISTOGRAM_MACROS_H_

#include "ash/ash_export.h"
#include "base/metrics/histogram_macros.h"

// Use these instead of UMA_HISTOGRAM_PERCENTAGE if these histogram needs to be
// recorded separately in tablet/clamshell mode.
// TODO(oshima): Create a macro which record both that works with presubmit.
// crbug.com/923159.

#define UMA_HISTOGRAM_PERCENTAGE_IN_TABLET(name, ...) \
  do {                                                \
    if (ash::InTabletMode())                          \
      UMA_HISTOGRAM_PERCENTAGE(name, __VA_ARGS__);    \
  } while (0)

#define UMA_HISTOGRAM_PERCENTAGE_IN_SPLITVIEW(in_split_view, name, ...) \
  do {                                                                  \
    if (in_split_view)                                                  \
      UMA_HISTOGRAM_PERCENTAGE(name, __VA_ARGS__);                      \
  } while (0)

#define UMA_HISTOGRAM_PERCENTAGE_IN_TABLET_NON_SPLITVIEW(in_split_view, name, \
                                                         ...)                 \
  do {                                                                        \
    if (ash::InTabletMode() && !in_split_view)                                \
      UMA_HISTOGRAM_PERCENTAGE(name, __VA_ARGS__);                            \
  } while (0)

#define UMA_HISTOGRAM_PERCENTAGE_IN_CLAMSHELL(name, ...) \
  do {                                                   \
    if (!ash::InTabletMode())                            \
      UMA_HISTOGRAM_PERCENTAGE(name, __VA_ARGS__);       \
  } while (0)

namespace ash {

ASH_EXPORT bool InTabletMode();

}  // namespace ash

#endif  // ASH_METRICS_HISTOGRAM_MACROS_H_
