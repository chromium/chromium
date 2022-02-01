// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TIME_CALENDAR_METRICS_H_
#define ASH_SYSTEM_TIME_CALENDAR_METRICS_H_

namespace ui {
class Event;
}  // namespace ui

namespace ash {

// The different hosts which hold components allowing a user to open the
// calendar. These are used in histograms, do not remove/renumber entries. If
// you're adding to this enum with the intention that it will be logged, update
// the CalendarViewShowSource token variant in histograms.xml.
enum class CalendarViewShowSource {
  kTimeView = 0,  // Shown via activating the time view in the status area.
  kDateView = 1,  // Shown via activating the  date view in the quick settings
                  // bubble.
  kAccelerator = 2,  // Shown via activating the accelerator.
  kMaxValue = kAccelerator
};

// Records calendar show metrics for a given CalendarViewShowSource
void RecordCalendarShowMetrics(CalendarViewShowSource show_source,
                               const ui::Event& event);

}  // namespace ash

#endif  // ASH_SYSTEM_TIME_CALENDAR_METRICS_H_
