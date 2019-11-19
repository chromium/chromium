// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_VALUE_EVENT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_VALUE_EVENT_H_

#include <ostream>
#include <vector>

#include "base/values.h"

namespace arc {

// Tracing event with a value.
struct ArcValueEvent {
  enum class Type {
    kMemTotal,
    kMemUsed,
    kSwapRead,
    kSwapWrite,
    kSwapWait,
    kGemObjects,
    kGemSize,
    kGpuFrequency,
    kCpuTemperature,
    kCpuFrequency,
    kCpuPower,
    kGpuPower,
    kMemoryPower,
  };

  ArcValueEvent(int64_t timestamp, Type type, int value);

  bool operator==(const ArcValueEvent& other) const;

  uint64_t timestamp;
  Type type;
  /**
   * kMemTotal - kb.
   * kMemUsed - kb.
   * kSwapRead - number of sectors.
   * kSwapWrite - number of sectors.
   * kSwapWait - milliseconds.
   * kGemObjects - number of objects
   * kGemSize - kb
   * kGpuFrequency - mhz
   * kCpuTemperature - celsius * 1000
   * kCpuFrequency - khz
   * kCpuPower - milli-watts
   * kGpuPower - milli-watts
   * kMemporyPower - milli-watts
   */
  int value;
};

using ValueEvents = std::vector<ArcValueEvent>;

// Serializes value events into |base::ListValue|.
base::ListValue SerializeValueEvents(const ValueEvents& value_events);

// Loads value events from |base::ListValue|. Returns true in case value
// events were loaded successfully.
bool LoadValueEvents(const base::Value* value, ValueEvents* value_events);

std::ostream& operator<<(std::ostream& os, ArcValueEvent::Type event_type);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_VALUE_EVENT_H_
