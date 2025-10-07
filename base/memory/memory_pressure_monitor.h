// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_MEMORY_PRESSURE_MONITOR_H_
#define BASE_MEMORY_MEMORY_PRESSURE_MONITOR_H_

#include "base/base_export.h"
#include "base/functional/callback.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/time/time.h"

namespace base {

enum class MemoryPressureMonitorTag {
  kTest = 0,
  kGlicProfileManager = 1,
  kLongScreenshotsTabService = 2,
  kPaintPreviewTabService = 3,
  kTabLoader = 4,
  kTabHoverCardController = 5,
  kWebUIContentsPreloadManager = 6,
  kOnDeviceTailModelService = 7,
  kPlayerCompositorDelegate = 8,
  kFrameEvictionManager = 9,
  kPrerenderHostRegistry = 10,
  kSubframeShutdownDelay = 11,
  kSpareRendererHostManager = 12,
  kUsbDeviceLinux = 13,
  kMax,
};

// TODO(chrisha): Make this a concrete class with per-OS implementations rather
// than an abstract base class.

// Declares the interface for a MemoryPressureMonitor. There are multiple
// OS specific implementations of this class. An instance of the memory
// pressure observer is created at the process level, tracks memory usage, and
// pushes memory state change notifications to the static function
// base::MemoryPressureListener::NotifyMemoryPressure. This is turn notifies
// all MemoryPressureListener instances via a callback.
class BASE_EXPORT MemoryPressureMonitor {
 public:
  using DispatchCallback =
      base::RepeatingCallback<void(MemoryPressureLevel level)>;

  MemoryPressureMonitor(const MemoryPressureMonitor&) = delete;
  MemoryPressureMonitor& operator=(const MemoryPressureMonitor&) = delete;

  virtual ~MemoryPressureMonitor();

  // Return the singleton MemoryPressureMonitor.
  static MemoryPressureMonitor* Get();

  // Returns the currently observed memory pressure.
  virtual MemoryPressureLevel GetCurrentPressureLevel(
      base::MemoryPressureMonitorTag tag) const = 0;

 protected:
  MemoryPressureMonitor();
};

}  // namespace base

#endif  // BASE_MEMORY_MEMORY_PRESSURE_MONITOR_H_
