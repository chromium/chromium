// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_SYSTEM_MODEL_H_
#define CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_SYSTEM_MODEL_H_

#include <map>
#include <string>

#include "base/values.h"
#include "chrome/browser/chromeos/arc/tracing/arc_cpu_event.h"
#include "chrome/browser/chromeos/arc/tracing/arc_value_event.h"

namespace arc {

// Contains information about system activity and involved threads. System
// activity includes CPU and memory events.
class ArcSystemModel {
 public:
  static constexpr int kUnknownPid = -1;

  struct ThreadInfo {
    ThreadInfo();
    ThreadInfo(int pid, const std::string& name);

    bool operator==(const ThreadInfo& other) const;

    // Process id or |kUnknownPid| if unknown.
    int pid = kUnknownPid;
    // Name of thread of process in case thread is main thread of the process.
    std::string name;
  };

  using ThreadMap = std::map<int, ThreadInfo>;

  ArcSystemModel();
  ~ArcSystemModel();

  void Reset();
  // Trims the model using |trim_timestamp|. Events before
  // |trim_timestamp| are consolidated with their timestamps aligned
  // to |trim_timestamp|. Events on or after |trim_timestamp| are left
  // in the model unchanged.
  void Trim(uint64_t trim_timestamp);

  // Closes range for each value event type by extending the latest value till
  // the |max_timestamp|.
  void CloseRangeForValueEvents(uint64_t max_timestamp);

  void CopyFrom(const ArcSystemModel& other);
  base::DictionaryValue Serialize() const;
  bool Load(const base::Value* root);

  bool operator==(const ArcSystemModel& other) const;

  ThreadMap& thread_map() { return thread_map_; }
  const ThreadMap& thread_map() const { return thread_map_; }

  AllCpuEvents& all_cpu_events() { return all_cpu_events_; }
  const AllCpuEvents& all_cpu_events() const { return all_cpu_events_; }

  ValueEvents& memory_events() { return memory_events_; }
  const ValueEvents& memory_events() const { return memory_events_; }

 private:
  ThreadMap thread_map_;
  AllCpuEvents all_cpu_events_;
  // TODO(khmel): For simplification and performance use separate channels
  // for each event type.
  ValueEvents memory_events_;

  DISALLOW_COPY_AND_ASSIGN(ArcSystemModel);
};

}  // namespace arc

#endif  // CHROME_BROWSER_CHROMEOS_ARC_TRACING_ARC_SYSTEM_MODEL_H_
