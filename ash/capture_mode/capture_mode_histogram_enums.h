// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_MODE_HISTOGRAM_ENUMS_H_
#define ASH_CAPTURE_MODE_CAPTURE_MODE_HISTOGRAM_ENUMS_H_

namespace ash {

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

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_MODE_HISTOGRAM_ENUMS_H_
