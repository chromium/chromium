// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_IME_IME_SWITCH_TYPE_H_
#define ASH_IME_IME_SWITCH_TYPE_H_

namespace ash {

// Used for histograms. See tools/metrics/histograms/enums.xml IMESwitchType.
enum class ImeSwitchType {
  // The values should not reordered or deleted and new entries should only be
  // added at the end (otherwise it will cause problems interpreting logs)
  kTray = 0,
  kAccelerator = 1,
  kModeChangeKey = 2,
  kMaxValue = kModeChangeKey,
};

}  // namespace ash

#endif  // ASH_IME_IME_SWITCH_TYPE_H_
