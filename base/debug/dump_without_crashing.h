// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_DEBUG_DUMP_WITHOUT_CRASHING_H_
#define BASE_DEBUG_DUMP_WITHOUT_CRASHING_H_

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/location.h"
#include "base/time/time.h"
#include "build/build_config.h"

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class DumpWithoutCrashingStatus {
  kThrottled,
  kUploaded,
  kMaxValue = kUploaded
};

namespace base {

namespace debug {

// Handler to silently dump the current process without crashing.
// Before calling this function, call SetDumpWithoutCrashingFunction to pass a
// function pointer.
// Windows:
// This must be done for each instance of base (i.e. module) and is normally
// chrome_elf!DumpProcessWithoutCrash. See example code in chrome_main.cc that
// does this for chrome.dll and chrome_child.dll. Note: Crashpad sets this up
// for main chrome.exe as part of calling crash_reporter::InitializeCrashpad.
// Mac/Linux:
// Crashpad does this as part of crash_reporter::InitializeCrashpad.
// Returns false if called before SetDumpWithoutCrashingFunction.
//
// This function must not be called with a tail call because that would cause
// the caller to be omitted from the call stack in the crash dump, and that is
// confusing and omits what is likely the most important context.
//
// Note: Calls to this function will not be throttled. To avoid performance
// problems if this is called many times in quick succession, prefer using one
// of the below variants.
NOT_TAIL_CALLED BASE_EXPORT bool DumpWithoutCrashingUnthrottled();

// Handler to silently dump the current process without crashing, that keeps
// track of call location so some throttling can be applied to avoid very
// frequent dump captures, which can have side-effects.
// `location` Location of the file from where the function is called.
// `time_between_dumps` Time until the next dump should be captured.
NOT_TAIL_CALLED BASE_EXPORT bool DumpWithoutCrashing(
    const base::Location& location = base::Location::Current(),
    base::TimeDelta time_between_dumps = base::Days(1));

// Handler to silently dump the current process without crashing that takes a
// location and unique id to keep a track and apply throttling. This function
// should be used when a domain wishes to capture dumps for multiple, unique
// reasons from a single location. An example would be unique bad mojo
// messages, or a value outside an expected range and the value should be
// considered interesting in the dump. The goal is to allow a single call to
// generate multiple dumps as needed and throttle future instances of the same
// identifiers for a short period of time.
// `unique_identifier` Hash to uniquely identify the function call. Consider
// using base::FastHash to generate the hash.
// `location` Location of the file from where the function is called.
// `time_between_dumps` Time until the next dump should be captured.
// Note:
// - The unique identifier, as of now, is not comparable across different
//   runs or builds and is stable only for a process lifetime.
// - The unique identifier is not recorded in the crash report. See
//   crash_logging.h for such a purpose.
NOT_TAIL_CALLED BASE_EXPORT bool DumpWithoutCrashingWithUniqueId(
    size_t unique_identifier,
    const base::Location& location = base::Location::Current(),
    base::TimeDelta time_between_dumps = base::Days(1));

// Sets a function that'll be invoked to dump the current process when
// DumpWithoutCrashing* is called. May be called with null to remove a
// previously set function.
BASE_EXPORT void SetDumpWithoutCrashingFunction(void (*function)());

// Clear both maps used to throttle calls to DumpWithoutCrashing for testing.
BASE_EXPORT void ClearMapsForTesting();

}  // namespace debug
}  // namespace base

#endif  // BASE_DEBUG_DUMP_WITHOUT_CRASHING_H_
