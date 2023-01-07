// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// MemoryPressure provides static APIs for handling memory pressure on
// platforms that have such signals, such as Android and ChromeOS.
// The app will try to discard buffers that aren't deemed essential (individual
// modules will implement their own policy).

#include "base/trace_event/memory_pressure_level_proto.h"

#include "base/tracing/protos/chrome_track_event.pbzero.h"  // nogncheck

namespace base {
namespace trace_event {

perfetto::protos::pbzero::MemoryPressureLevel MemoryPressureLevelToTraceEnum(
    MemoryPressureListener::MemoryPressureLevel memory_pressure_level) {
  using ProtoLevel = perfetto::protos::pbzero::MemoryPressureLevel;
  switch (memory_pressure_level) {
    case MemoryPressureListener::MEMORY_PRESSURE_LEVEL_NONE:
      return ProtoLevel::MEMORY_PRESSURE_LEVEL_NONE;
    case MemoryPressureListener::MEMORY_PRESSURE_LEVEL_MODERATE:
      return ProtoLevel::MEMORY_PRESSURE_LEVEL_MODERATE;
    case MemoryPressureListener::MEMORY_PRESSURE_LEVEL_CRITICAL:
      return ProtoLevel::MEMORY_PRESSURE_LEVEL_CRITICAL;
  }
}

}  // namespace trace_event
}  // namespace base
