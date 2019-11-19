// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_CPU_EVENT_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_CPU_EVENT_H_

#include <ostream>
#include <vector>

#include "base/values.h"

namespace arc {

// Keeps information about CPU event
struct ArcCpuEvent {
  enum class Type {
    kIdleIn = 0,  // CPU is in idle state.
    kIdleOut,     // CPU is no longer in idle state.
    kWakeUp,      // Wake up with a task.
    kActive,      // Task is started on CPU.
  };

  ArcCpuEvent(uint64_t timestamp, Type type, uint32_t tid);

  bool operator==(const ArcCpuEvent& other) const;

  uint64_t timestamp;
  Type type;
  uint32_t tid;
};

using CpuEvents = std::vector<ArcCpuEvent>;
// Represents CPU events for all CPUs in the system. CPUs id is in the range
// 0..N and corresponds to the index in |AllCpuEvents|, N is CPU count - 1.
using AllCpuEvents = std::vector<CpuEvents>;

// Helper that adds CPU event into |cpu_events|. Returns true in case event was
// created and added or false if it breaks any constraint.
bool AddCpuEvent(CpuEvents* cpu_events,
                 uint64_t timestamp,
                 ArcCpuEvent::Type type,
                 uint32_t tid);

// Helper that adds next CPU event to |all_cpu_events|. If needed new entry
// |CpuEvents| is allocated for |cpu_id|. Returns true in case event was
// created and added or false if it breaks any constraint.
bool AddAllCpuEvent(AllCpuEvents* all_cpu_events,
                    uint32_t cpu_id,
                    uint64_t timestamp,
                    ArcCpuEvent::Type type,
                    uint32_t tid);

// Serializes CPU events into |base::ListValue|.
base::ListValue SerializeCpuEvents(const CpuEvents& cpu_events);
// Serializes all CPU events into |base::ListValue|.
base::ListValue SerializeAllCpuEvents(const AllCpuEvents& all_cpu_events);

// Loads CPU events from |base::ListValue|. Returns true in case CPU events were
// loaded successfully.
bool LoadCpuEvents(const base::Value* value, CpuEvents* cpu_events);
// Loads all CPU events from |base::ListValue|. Returns true in case CPU events
// were loaded successfully.
bool LoadAllCpuEvents(const base::Value* value, AllCpuEvents* all_cpu_events);

std::ostream& operator<<(std::ostream& os, ArcCpuEvent::Type event_type);

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_CPU_EVENT_H_
