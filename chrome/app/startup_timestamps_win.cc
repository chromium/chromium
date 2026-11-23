// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/startup_timestamps_win.h"

#include <windows.h>

#include <stdint.h>

#include "base/command_line.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/common/chrome_switches.h"

namespace {

base::TimeDelta GetCurrentQpcDelta() {
  // Ensure `TimeTicks` is initialized so `TimeDelta::FromQPCValue()` has a
  // valid cached QPC frequency in //base.
  (void)base::TimeTicks::IsHighResolution();
  LARGE_INTEGER current_qpc = {};
  ::QueryPerformanceCounter(&current_qpc);
  return base::TimeDelta::FromQPCValue(current_qpc.QuadPart);
}

base::TimeDelta GetApplicationStartQpcDeltaImpl(
    const base::CommandLine& command_line,
    std::string_view process_type,
    base::TimeDelta current_qpc_delta) {
  if (process_type.empty() && command_line.HasSwitch(switches::kIsolated) &&
      command_line.HasSwitch(switches::kIsolatedStubStartQpcUs)) {
    int64_t stub_qpc_us = 0;
    if (base::StringToInt64(
            command_line.GetSwitchValueASCII(switches::kIsolatedStubStartQpcUs),
            &stub_qpc_us) &&
        stub_qpc_us > 0) {
      const base::TimeDelta stub_delta = base::Microseconds(stub_qpc_us);
      if (stub_delta <= current_qpc_delta &&
          current_qpc_delta - stub_delta <= kMaxEntryPointQpcAge) {
        return stub_delta;
      }
    }
  }
  return current_qpc_delta;
}

base::TimeTicks ResolveEntryPointQpcDeltaImpl(
    base::TimeDelta qpc_delta,
    base::TimeTicks now,
    bool is_high_resolution,
    base::TimeDelta current_qpc_delta) {
  if (!qpc_delta.is_positive()) {
    return now;
  }
  if (is_high_resolution) {
    return base::TimeTicks() + qpc_delta;
  }
  const base::TimeDelta elapsed = current_qpc_delta - qpc_delta;
  if (elapsed.is_negative() || elapsed > kMaxEntryPointQpcAge ||
      elapsed >= now.since_origin()) {
    return now;
  }
  return now - elapsed;
}

}  // namespace

base::TimeDelta GetApplicationStartQpcDelta(
    const base::CommandLine& command_line,
    std::string_view process_type) {
  return GetApplicationStartQpcDeltaImpl(command_line, process_type,
                                         GetCurrentQpcDelta());
}

base::TimeTicks ResolveEntryPointQpcDelta(base::TimeDelta qpc_delta,
                                          base::TimeTicks now) {
  if (base::TimeTicks::IsHighResolution()) {
    return ResolveEntryPointQpcDeltaImpl(qpc_delta, now,
                                         /*is_high_resolution=*/true,
                                         base::TimeDelta());
  }
  return ResolveEntryPointQpcDeltaImpl(qpc_delta, now,
                                       /*is_high_resolution=*/false,
                                       GetCurrentQpcDelta());
}

base::TimeDelta GetApplicationStartQpcDeltaForTesting(
    const base::CommandLine& command_line,
    std::string_view process_type,
    base::TimeDelta current_qpc_delta) {
  return GetApplicationStartQpcDeltaImpl(command_line, process_type,
                                         current_qpc_delta);
}

base::TimeTicks ResolveEntryPointQpcDeltaForTesting(
    base::TimeDelta qpc_delta,
    base::TimeTicks now,
    bool is_high_resolution,
    base::TimeDelta current_qpc_delta) {
  return ResolveEntryPointQpcDeltaImpl(qpc_delta, now, is_high_resolution,
                                       current_qpc_delta);
}
