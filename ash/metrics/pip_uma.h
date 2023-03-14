// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_PIP_UMA_H_
#define ASH_METRICS_PIP_UMA_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

constexpr char kAshPipEventsHistogramName[] = "Ash.Pip.Events";
constexpr char kAshPipAndroidPipUseTimeHistogramName[] =
    "Ash.Pip.AndroidPipUseTime";

// This enum should be kept in sync with the AshPipEvents enum in
// src/tools/metrics/histograms/enums.xml.
enum class AshPipEvents {
  PIP_START = 0,
  PIP_END = 1,
  ANDROID_PIP_START = 2,
  ANDROID_PIP_END = 3,
  CHROME_PIP_START = 4,
  CHROME_PIP_END = 5,
  FREE_RESIZE = 6,
  CHROME_RESIZE_HANDLE_RESIZE = 7,
  kMaxValue = CHROME_RESIZE_HANDLE_RESIZE
};

// This enum should be kept in sync with the AshPipPosition enum in
// src/tools/metrics/histograms/enums.xml.
enum class AshPipPosition {
  MIDDLE = 0,
  TOP_MIDDLE = 1,
  MIDDLE_LEFT = 2,
  MIDDLE_RIGHT = 3,
  BOTTOM_MIDDLE = 4,
  TOP_LEFT = 5,
  TOP_RIGHT = 6,
  BOTTOM_LEFT = 7,
  BOTTOM_RIGHT = 8,
  kMaxValue = BOTTOM_RIGHT
};

}  // namespace ash

#endif  // ASH_METRICS_PIP_UMA_H_
