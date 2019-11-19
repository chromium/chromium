// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/test_timeouts.h"

#include <algorithm>
#include <string>

#include "base/clang_coverage_buildflags.h"
#include "base/command_line.h"
#include "base/debug/debugger.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/test_switches.h"
#include "build/build_config.h"

namespace {

// Sets value to the greatest of:
// 1) value's current value multiplied by kTimeoutMultiplier (assuming
// InitializeTimeout is called only once per value).
// 2) min_value.
// 3) the numerical value given by switch_name on the command line multiplied
// by kTimeoutMultiplier.
void InitializeTimeout(const char* switch_name, int min_value, int* value) {
  DCHECK(value);
  int command_line_timeout = 0;
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switch_name)) {
    std::string string_value(base::CommandLine::ForCurrentProcess()->
         GetSwitchValueASCII(switch_name));
    if (!base::StringToInt(string_value, &command_line_timeout)) {
      LOG(FATAL) << "Timeout value \"" << string_value << "\" was parsed as "
                 << command_line_timeout;
    }
  }

#if defined(MEMORY_SANITIZER)
  // ASan/TSan/MSan instrument each memory access. This may slow the execution
  // down significantly.
  // For MSan the slowdown depends heavily on the value of msan_track_origins
  // build flag. The multiplier below corresponds to msan_track_origins = 1.
#if defined(OS_CHROMEOS)
  // A handful of tests on ChromeOS run *very* close to the 6x limit used
  // else where, so it's bumped to 7x.
  constexpr int kTimeoutMultiplier = 7;
#else
  constexpr int kTimeoutMultiplier = 6;
#endif
#elif defined(ADDRESS_SANITIZER) && defined(OS_WIN)
  // ASan/Win has not been optimized yet, give it a higher
  // timeout multiplier. See http://crbug.com/412471
  constexpr int kTimeoutMultiplier = 3;
#elif defined(ADDRESS_SANITIZER) && defined(OS_CHROMEOS)
  // A number of tests on ChromeOS run very close to the 2x limit, so ChromeOS
  // gets 3x.
  constexpr int kTimeoutMultiplier = 3;
#elif defined(ADDRESS_SANITIZER) || defined(THREAD_SANITIZER)
  constexpr int kTimeoutMultiplier = 2;
#elif BUILDFLAG(CLANG_COVERAGE)
  // On coverage build, tests run 3x slower.
  constexpr int kTimeoutMultiplier = 3;
#else
  constexpr int kTimeoutMultiplier = 1;
#endif

  *value = std::max(std::max(*value, command_line_timeout) * kTimeoutMultiplier,
                    min_value);
}

}  // namespace

// static
bool TestTimeouts::initialized_ = false;

// The timeout values should increase in the order they appear in this block.
// static
int TestTimeouts::tiny_timeout_ms_ = 100;
int TestTimeouts::action_timeout_ms_ = 10000;
int TestTimeouts::action_max_timeout_ms_ = 30000;
int TestTimeouts::test_launcher_timeout_ms_ = 45000;

// static
void TestTimeouts::Initialize() {
  DCHECK(!initialized_);
  initialized_ = true;

  const bool being_debugged = base::debug::BeingDebugged();
  if (being_debugged) {
    fprintf(stdout,
        "Detected presence of a debugger, running without test timeouts.\n");
  }

  // Note that these timeouts MUST be initialized in the correct order as
  // per the CHECKS below.

  InitializeTimeout(switches::kTestTinyTimeout, 0, &tiny_timeout_ms_);

  // All timeouts other than the "tiny" one should be set to very large values
  // when in a debugger or when run interactively, so that tests will not get
  // auto-terminated.  By setting the UI test action timeout to at least this
  // value, we guarantee the subsequent timeouts will be this large also.
  // Setting the "tiny" timeout to a large value as well would make some tests
  // hang (because it's used as a task-posting delay).  In particular this
  // causes problems for some iOS device tests, which are always run inside a
  // debugger (thus BeingDebugged() is true even on the bots).
  int min_ui_test_action_timeout = tiny_timeout_ms_;
  if (being_debugged || base::CommandLine::ForCurrentProcess()->HasSwitch(
                            switches::kTestLauncherInteractive)) {
    constexpr int kVeryLargeTimeoutMs = 100'000'000;
    min_ui_test_action_timeout = kVeryLargeTimeoutMs;
  }

  InitializeTimeout(switches::kUiTestActionTimeout, min_ui_test_action_timeout,
                    &action_timeout_ms_);
  InitializeTimeout(switches::kUiTestActionMaxTimeout, action_timeout_ms_,
                    &action_max_timeout_ms_);

  // Test launcher timeout is independent from anything above action timeout.
  InitializeTimeout(switches::kTestLauncherTimeout, action_timeout_ms_,
                    &test_launcher_timeout_ms_);

  // The timeout values should be increasing in the right order.
  CHECK_LE(tiny_timeout_ms_, action_timeout_ms_);
  CHECK_LE(action_timeout_ms_, action_max_timeout_ms_);
  CHECK_LE(action_timeout_ms_, test_launcher_timeout_ms_);
}
