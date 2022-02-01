// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_metrics.h"

#include "base/check_op.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "ui/events/event.h"

namespace ash {

namespace {

// The different ways in which a CalendarViewShowSource can be activated. These
// are used in histograms, do not remove/renumber entries. If you're adding to
// this enum with the intention that it will be logged, update the
// CalendarViewShowMethod listing in enums.xml.
enum class CalendarViewShowMethod {
  kInvalid = 0,
  kTap = 1,
  kClick = 2,
  kKeyboard = 3,
  kStylus = 4,
  kMaxValue = kStylus
};

constexpr char kCalendarViewShowSourcePrefix[] = "Ash.Calendar.ShowSource.";

}  // namespace

void RecordCalendarShowMetrics(CalendarViewShowSource show_source,
                               const ui::Event& event) {
  std::string histogram_name = kCalendarViewShowSourcePrefix;
  switch (show_source) {
    case CalendarViewShowSource::kDateView:
      histogram_name += "DateView";
      break;
    case CalendarViewShowSource::kTimeView:
      histogram_name += "TimeView";
      break;
    case CalendarViewShowSource::kAccelerator:
      DCHECK(event.IsKeyEvent());
      histogram_name += "Keyboard";
      break;
  }

  if (event.IsGestureEvent()) {
    base::UmaHistogramEnumeration(histogram_name, CalendarViewShowMethod::kTap);
  } else if (event.IsMouseEvent()) {
    base::UmaHistogramEnumeration(histogram_name,
                                  CalendarViewShowMethod::kClick);
  } else if (event.IsKeyEvent()) {
    base::UmaHistogramEnumeration(histogram_name,
                                  CalendarViewShowMethod::kKeyboard);
  } else if (event.IsTouchEvent()) {
    base::UmaHistogramEnumeration(histogram_name,
                                  CalendarViewShowMethod::kStylus);
  } else {
    base::UmaHistogramEnumeration(histogram_name,
                                  CalendarViewShowMethod::kInvalid);
    NOTREACHED();
  }
}

}  // namespace ash