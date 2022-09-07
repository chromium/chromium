// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_METRICS_POINTER_METRICS_RECORDER_H_
#define ASH_METRICS_POINTER_METRICS_RECORDER_H_

#include "ash/ash_export.h"
#include "ui/events/event_handler.h"

namespace ash {

// Form factor of the down event.
// This enum is used to control a UMA histogram buckets. If you change this
// enum, you should update DownEventMetric as well.
enum class DownEventFormFactor {
  kClamshell = 0,
  kTabletModeLandscape,
  kTabletModePortrait,
  kFormFactorCount,
};

// Input type of the down event.
// This enum is used to control a UMA histogram buckets. If you change this
// enum, you should update DownEventMetric as well.
enum class DownEventSource {
  kUnknown = 0,  // Deprecated, never occurs in practice.
  kMouse,
  kStylus,
  kTouch,
  kSourceCount,
};

// App type (Destination), Input and FormFactor Combination of the down event.
// This enum is used to back an UMA histogram and new values should
// be inserted immediately above kMaxValue.
enum class DownEventMetric2 {
  // All "Unknown" types are deprecated, never occur in practice.
  kNonAppUnknownClamshell = 0,
  kNonAppUnknownTabletLandscape = 1,
  kNonAppUnknownTabletPortrait = 2,
  kNonAppMouseClamshell = 3,
  kNonAppMouseTabletLandscape = 4,
  kNonAppMouseTabletPortrait = 5,
  kNonAppStylusClamshell = 6,
  kNonAppStylusTabletLandscape = 7,
  kNonAppStylusTabletPortrait = 8,
  kNonAppTouchClamshell = 9,
  kNonAppTouchTabletLandscape = 10,
  kNonAppTouchTabletPortrait = 11,
  kBrowserUnknownClamshell = 12,
  kBrowserUnknownTabletLandscape = 13,
  kBrowserUnknownTabletPortrait = 14,
  kBrowserMouseClamshell = 15,
  kBrowserMouseTabletLandscape = 16,
  kBrowserMouseTabletPortrait = 17,
  kBrowserStylusClamshell = 18,
  kBrowserStylusTabletLandscape = 19,
  kBrowserStylusTabletPortrait = 20,
  kBrowserTouchClamshell = 21,
  kBrowserTouchTabletLandscape = 22,
  kBrowserTouchTabletPortrait = 23,
  kChromeAppUnknownClamshell = 24,
  kChromeAppUnknownTabletLandscape = 25,
  kChromeAppUnknownTabletPortrait = 26,
  kChromeAppMouseClamshell = 27,
  kChromeAppMouseTabletLandscape = 28,
  kChromeAppMouseTabletPortrait = 29,
  kChromeAppStylusClamshell = 30,
  kChromeAppStylusTabletLandscape = 31,
  kChromeAppStylusTabletPortrait = 32,
  kChromeAppTouchClamshell = 33,
  kChromeAppTouchTabletLandscape = 34,
  kChromeAppTouchTabletPortrait = 35,
  kArcAppUnknownClamshell = 36,
  kArcAppUnknownTabletLandscape = 37,
  kArcAppUnknownTabletPortrait = 38,
  kArcAppMouseClamshell = 39,
  kArcAppMouseTabletLandscape = 40,
  kArcAppMouseTabletPortrait = 41,
  kArcAppStylusClamshell = 42,
  kArcAppStylusTabletLandscape = 43,
  kArcAppStylusTabletPortrait = 44,
  kArcAppTouchClamshell = 45,
  kArcAppTouchTabletLandscape = 46,
  kArcAppTouchTabletPortrait = 47,
  kCrostiniAppUnknownClamshell = 48,
  kCrostiniAppUnknownTabletLandscape = 49,
  kCrostiniAppUnknownTabletPortrait = 50,
  kCrostiniAppMouseClamshell = 51,
  kCrostiniAppMouseTabletLandscape = 52,
  kCrostiniAppMouseTabletPortrait = 53,
  kCrostiniAppStylusClamshell = 54,
  kCrostiniAppStylusTabletLandscape = 55,
  kCrostiniAppStylusTabletPortrait = 56,
  kCrostiniAppTouchClamshell = 57,
  kCrostiniAppTouchTabletLandscape = 58,
  kCrostiniAppTouchTabletPortrait = 59,
  kSystemAppUnknownClamshell = 60,
  kSystemAppUnknownTabletLandscape = 61,
  kSystemAppUnknownTabletPortrait = 62,
  kSystemAppMouseClamshell = 63,
  kSystemAppMouseTabletLandscape = 64,
  kSystemAppMouseTabletPortrait = 65,
  kSystemAppStylusClamshell = 66,
  kSystemAppStylusTabletLandscape = 67,
  kSystemAppStylusTabletPortrait = 68,
  kSystemAppTouchClamshell = 69,
  kSystemAppTouchTabletLandscape = 70,
  kSystemAppTouchTabletPortrait = 71,
  kMaxValue = kSystemAppTouchTabletPortrait
};

// A metrics recorder that records pointer related metrics.
class ASH_EXPORT PointerMetricsRecorder : public ui::EventHandler {
 public:
  PointerMetricsRecorder();

  PointerMetricsRecorder(const PointerMetricsRecorder&) = delete;
  PointerMetricsRecorder& operator=(const PointerMetricsRecorder&) = delete;

  ~PointerMetricsRecorder() override;

  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnTouchEvent(ui::TouchEvent* event) override;
};

}  // namespace ash

#endif  // ASH_METRICS_POINTER_METRICS_RECORDER_H_
