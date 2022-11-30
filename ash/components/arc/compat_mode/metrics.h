// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_COMPAT_MODE_METRICS_H_
#define ASH_COMPONENTS_ARC_COMPAT_MODE_METRICS_H_

#include "ash/components/arc/mojom/compatibility_mode.mojom.h"

namespace arc {

enum class ResizeLockActionType {
  ResizeToPhone,
  ResizeToTablet,
  TurnOnResizeLock,
  TurnOffResizeLock,
};

enum class ResizeLockStateHistogramType {
  InitialState,
};

// The assigned values cannot be renumbered or reordered because it's used for
// enum histogram.
enum class RightClickConversionResultHistogramResult {
  kDisabled = 0,
  kConverted = 1,
  kNotConverted = 2,
  kMaxValue = kNotConverted,
};

void RecordResizeLockAction(ResizeLockActionType type);

void RecordResizeLockStateHistogram(ResizeLockStateHistogramType type,
                                    mojom::ArcResizeLockState state);

void RecordRightClickConversionResultHistogram(
    RightClickConversionResultHistogramResult result);

const char* GetResizeLockActionNameForTesting(ResizeLockActionType type);
const char* GetResizeLockStateHistogramNameForTesting(
    ResizeLockStateHistogramType type);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_COMPAT_MODE_METRICS_H_
