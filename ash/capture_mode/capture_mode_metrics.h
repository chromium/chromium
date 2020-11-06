// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_

namespace ash {

// Enumeration of capture bar buttons that can be pressed while in capture mode.
// Note that these values are persisted to histograms so existing values should
// remain unchanged and new values should be added to the end.
enum class CaptureModeBarButtonType {
  kScreenCapture,
  kScreenRecord,
  kFull,
  kRegion,
  kWindow,
  kExit,
  kMaxValue = kExit,
};

// Enumeration of actions that can be taken to enter capture mode. Note that
// these values are persisted to histograms so existing values should remain
// unchanged and new values should be added to the end.
enum class CaptureModeEntryType {
  kAccelTakePartialScreenshot,
  kAccelTakeWindowScreenshot,
  kQuickSettings,
  kStylusPalette,
  kMaxValue = kStylusPalette,
};

// Records capture mode bar button presses given by |button_type|.
void RecordCaptureModeBarButtonType(CaptureModeBarButtonType button_type);

// Records the method the user enters capture mode given by |entry_type|.
void RecordCaptureModeEntryType(CaptureModeEntryType entry_type);

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_
