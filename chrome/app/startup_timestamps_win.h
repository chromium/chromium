// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_APP_STARTUP_TIMESTAMPS_WIN_H_
#define CHROME_APP_STARTUP_TIMESTAMPS_WIN_H_

#include <string_view>

#include "base/time/time.h"

namespace base {
class CommandLine;
}

// Maximum allowed elapsed time between the stub process entry point and the
// isolated browser entry point (or between chrome.exe and chrome.dll entry).
inline constexpr base::TimeDelta kMaxEntryPointQpcAge = base::Hours(1);

// Returns the process entry-point QPC timestamp (microseconds since the QPC
// epoch via `base::TimeDelta::FromQPCValue`). For an isolated browser launched
// by the stub (`process_type` empty and `--isolated`), substitutes
// `--isolated-stub-start-qpc-us` when valid and within `kMaxEntryPointQpcAge`.
base::TimeDelta GetApplicationStartQpcDelta(
    const base::CommandLine& command_line,
    std::string_view process_type);

// Converts a QPC-derived entry-point delta passed from chrome.exe into
// chrome.dll's `TimeTicks` clock domain. When `TimeTicks` is QPC-backed
// (`IsHighResolution()`), returns `base::TimeTicks() + qpc_delta`; otherwise,
// subtracts the elapsed QPC offset from `now`. Falls back to `now` if
// `qpc_delta` is invalid, the elapsed offset is out of range, or the adjusted
// timestamp would be null or negative.
base::TimeTicks ResolveEntryPointQpcDelta(base::TimeDelta qpc_delta,
                                          base::TimeTicks now);

// Test entry points allowing deterministic testing of switch validation and
// low-resolution clock conversion without depending on live QPC or hardware
// TimeTicks resolution.
base::TimeDelta GetApplicationStartQpcDeltaForTesting(
    const base::CommandLine& command_line,
    std::string_view process_type,
    base::TimeDelta current_qpc_delta);

base::TimeTicks ResolveEntryPointQpcDeltaForTesting(
    base::TimeDelta qpc_delta,
    base::TimeTicks now,
    bool is_high_resolution,
    base::TimeDelta current_qpc_delta);

#endif  // CHROME_APP_STARTUP_TIMESTAMPS_WIN_H_
