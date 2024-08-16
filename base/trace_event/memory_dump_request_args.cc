// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/memory_dump_request_args.h"

#include "base/notreached.h"

namespace base {
namespace trace_event {

// static
const char* MemoryDumpTypeToString(const MemoryDumpType& dump_type) {
  switch (dump_type) {
    case MemoryDumpType::kPeriodicInterval:
      return "periodic_interval";
    case MemoryDumpType::kExplicitlyTriggered:
      return "explicitly_triggered";
    case MemoryDumpType::kSummaryOnly:
      return "summary_only";
  }
  NOTREACHED();
}

MemoryDumpType StringToMemoryDumpType(const std::string& str) {
  if (str == "periodic_interval") {
    return MemoryDumpType::kPeriodicInterval;
  }
  if (str == "explicitly_triggered")
    return MemoryDumpType::kExplicitlyTriggered;
  if (str == "summary_only")
    return MemoryDumpType::kSummaryOnly;
  NOTREACHED();
}

const char* MemoryDumpLevelOfDetailToString(
    const MemoryDumpLevelOfDetail& level_of_detail) {
  switch (level_of_detail) {
    case MemoryDumpLevelOfDetail::kBackground:
      return "background";
    case MemoryDumpLevelOfDetail::kLight:
      return "light";
    case MemoryDumpLevelOfDetail::kDetailed:
      return "detailed";
  }
  NOTREACHED();
}

MemoryDumpLevelOfDetail StringToMemoryDumpLevelOfDetail(
    const std::string& str) {
  if (str == "background")
    return MemoryDumpLevelOfDetail::kBackground;
  if (str == "light")
    return MemoryDumpLevelOfDetail::kLight;
  if (str == "detailed")
    return MemoryDumpLevelOfDetail::kDetailed;
  NOTREACHED();
}

}  // namespace trace_event
}  // namespace base
