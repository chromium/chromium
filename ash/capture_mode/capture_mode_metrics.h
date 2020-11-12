// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_

#include <stdint.h>

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
  kPowerMenu,
  kMaxValue = kPowerMenu,
};

// Records capture mode bar button presses given by |button_type|.
void RecordCaptureModeBarButtonType(CaptureModeBarButtonType button_type);

// Records the method the user enters capture mode given by |entry_type|.
void RecordCaptureModeEntryType(CaptureModeEntryType entry_type);

// Records the number of times a user adjusts a capture region. This includes
// moving and resizing. The count is started when a user sets the capture source
// as a region. The count is recorded and reset when a user performs a capture.
// The count is just reset when a user selects a new region or the user switches
// capture sources.
void RecordNumberOfCaptureRegionAdjustments(int num_adjustments);

// Records the length in seconds of a recording taken by capture mode.
void RecordCaptureModeRecordTime(int64_t length_in_seconds);

// Records if the user has switched modes during a capture session.
void RecordCaptureModeSwitchesFromInitialMode(bool switched);

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_METRICS_H_
