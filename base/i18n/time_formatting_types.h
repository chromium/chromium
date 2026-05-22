// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_TIME_FORMATTING_TYPES_H_
#define BASE_I18N_TIME_FORMATTING_TYPES_H_

namespace base {

// Argument type used to specify the hour clock type.
enum HourClockType {
  k12HourClock,  // Uses 1-12. e.g., "3:07 PM"
  k24HourClock,  // Uses 0-23. e.g., "15:07"
};

// Argument type used to specify whether or not to include AM/PM sign.
enum AmPmClockType {
  kDropAmPm,  // Drops AM/PM sign. e.g., "3:07"
  kKeepAmPm,  // Keeps AM/PM sign. e.g., "3:07 PM"
};

// Should match UMeasureFormatWidth in measfmt.h; replicated here to avoid
// requiring third_party/icu dependencies with this file.
enum DurationFormatWidth {
  DURATION_WIDTH_WIDE,    // "3 hours, 7 minutes"
  DURATION_WIDTH_SHORT,   // "3 hr, 7 min"
  DURATION_WIDTH_NARROW,  // "3h 7m"
  DURATION_WIDTH_NUMERIC  // "3:07"
};

}  // namespace base

#endif  // BASE_I18N_TIME_FORMATTING_TYPES_H_
