// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/dump_without_crashing.h"

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/no_destructor.h"
#include "base/synchronization/lock.h"
#include "base/trace_event/base_tracing.h"

namespace {

// Pointer to the function that's called by DumpWithoutCrashing* to dump the
// process's memory.
void(CDECL* dump_without_crashing_function_)() = nullptr;

template <typename Map, typename Key>
bool ShouldDump(Map& map, Key& key, base::TimeDelta time_between_dumps) {
  static base::NoDestructor<base::Lock> lock;
  base::AutoLock auto_lock(*lock);
  base::TimeTicks now = base::TimeTicks::Now();
  auto [it, inserted] = map.emplace(key, now);
  if (inserted) {
    return true;
  }

  if (now - it->second >= time_between_dumps) {
    it->second = now;
    return true;
  }
  return false;
}

// This function takes `location` and `time_between_dumps` as an input
// and checks if DumpWithoutCrashing() meets the requirements to take the dump
// or not.
bool ShouldDumpWithoutCrashWithLocation(const base::Location& location,
                                        base::TimeDelta time_between_dumps) {
  static base::NoDestructor<std::map<base::Location, base::TimeTicks>>
      location_to_timestamp;
  return ShouldDump(*location_to_timestamp, location, time_between_dumps);
}

// Pair of `location` and `unique_identifier` creates a unique key and checks
// if DumpWithoutCrashingWithUniqueId() meets the requirements to take dump or
// not.
bool ShouldDumpWithoutCrashWithLocationAndUniqueId(
    size_t unique_identifier,
    const base::Location& location,
    base::TimeDelta time_between_dumps) {
  static base::NoDestructor<
      std::map<std::pair<base::Location, size_t>, base::TimeTicks>>
      location_and_unique_identifier_to_timestamp;
  std::pair<base::Location, size_t> key(location, unique_identifier);
  return ShouldDump(*location_and_unique_identifier_to_timestamp, key,
                    time_between_dumps);
}

}  // namespace

namespace base {

namespace debug {

bool DumpWithoutCrashingUnthrottled() {
  TRACE_EVENT0("base", "DumpWithoutCrashingUnthrottled");
  if (dump_without_crashing_function_) {
    (*dump_without_crashing_function_)();
    return true;
  }
  return false;
}

bool DumpWithoutCrashing(const base::Location& location,
                         base::TimeDelta time_between_dumps) {
  TRACE_EVENT0("base", "DumpWithoutCrashing");
  if (dump_without_crashing_function_ &&
      ShouldDumpWithoutCrashWithLocation(location, time_between_dumps)) {
    (*dump_without_crashing_function_)();
    base::UmaHistogramEnumeration("Stability.DumpWithoutCrashingStatus",
                                  DumpWithoutCrashingStatus::kUploaded);
    return true;
  }
  base::UmaHistogramEnumeration("Stability.DumpWithoutCrashingStatus",
                                DumpWithoutCrashingStatus::kThrottled);
  return false;
}

bool DumpWithoutCrashingWithUniqueId(size_t unique_identifier,
                                     const base::Location& location,
                                     base::TimeDelta time_between_dumps) {
  TRACE_EVENT0("base", "DumpWithoutCrashingWithUniqueId");
  if (dump_without_crashing_function_ &&
      ShouldDumpWithoutCrashWithLocationAndUniqueId(unique_identifier, location,
                                                    time_between_dumps)) {
    (*dump_without_crashing_function_)();
    base::UmaHistogramEnumeration("Stability.DumpWithoutCrashingStatus",
                                  DumpWithoutCrashingStatus::kUploaded);
    return true;
  }
  base::UmaHistogramEnumeration("Stability.DumpWithoutCrashingStatus",
                                DumpWithoutCrashingStatus::kThrottled);
  return false;
}

void SetDumpWithoutCrashingFunction(void (CDECL *function)()) {
#if !defined(COMPONENT_BUILD)
  // In component builds, the same base is shared between modules
  // so might be initialized several times. However in non-
  // component builds this should never happen.
  DCHECK(!dump_without_crashing_function_ || !function);
#endif
  dump_without_crashing_function_ = function;
}

}  // namespace debug
}  // namespace base
