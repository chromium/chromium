// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/debugger.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#include "testing/gtest/include/gtest/gtest.h"

namespace {

#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
void CrashWithBreakDebugger() {
  base::debug::SetSuppressDebugUI(false);
  base::debug::BreakDebugger();

#if BUILDFLAG(IS_WIN)
  // This should not be executed.
  _exit(125);
#endif
}
#endif  // defined(GTEST_HAS_DEATH_TEST)

}  // namespace

// Death tests misbehave on Android.
#if defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)

TEST(Debugger, CrashAtBreakpoint) {
  EXPECT_DEATH(CrashWithBreakDebugger(), "");
}

#if BUILDFLAG(IS_WIN)
TEST(Debugger, DoesntExecuteBeyondBreakpoint) {
  EXPECT_EXIT(CrashWithBreakDebugger(),
              ::testing::ExitedWithCode(STATUS_BREAKPOINT), "");
}
#endif  // BUILDFLAG(IS_WIN)

#else   // defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
TEST(Debugger, NoTest) {
}
#endif  // defined(GTEST_HAS_DEATH_TEST) && !BUILDFLAG(IS_ANDROID)
