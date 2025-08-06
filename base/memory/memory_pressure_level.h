// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_MEMORY_PRESSURE_LEVEL_H_
#define BASE_MEMORY_MEMORY_PRESSURE_LEVEL_H_

namespace base {

// A Java counterpart will be generated for this enum.
// The values needs to be kept in sync with the MemoryPressureLevel entry in
// enums.xml.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.base
// GENERATED_JAVA_PREFIX_TO_STRIP: MEMORY_PRESSURE_LEVEL_
enum MemoryPressureLevel {
  // No problems, there is enough memory to use. This event is not sent via
  // callback, but the enum is used in other places to find out the current
  // state of the system.
  MEMORY_PRESSURE_LEVEL_NONE = 0,

  // Modules are advised to free buffers that are cheap to re-allocate and not
  // immediately needed.
  MEMORY_PRESSURE_LEVEL_MODERATE = 1,

  // At this level, modules are advised to free all possible memory.  The
  // alternative is to be killed by the system, which means all memory will
  // have to be re-created, plus the cost of a cold start.
  MEMORY_PRESSURE_LEVEL_CRITICAL = 2,

  // This must be the last value in the enum. The casing is different from the
  // other values to make this enum work well with the
  // UMA_HISTOGRAM_ENUMERATION macro.
  kMaxValue = MEMORY_PRESSURE_LEVEL_CRITICAL,
};

}  // namespace base

#endif  // BASE_MEMORY_MEMORY_PRESSURE_LEVEL_H_
