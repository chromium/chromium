// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_CLANG_COVERAGE_H_
#define BASE_TEST_CLANG_COVERAGE_H_

#include "base/clang_coverage_buildflags.h"

#if !BUILDFLAG(CLANG_COVERAGE)
#error "Clang coverage can only be used if CLANG_COVERAGE macro is defined"
#endif

namespace base {

// Write out the accumulated code coverage profile to the configured file.
// This is used internally by e.g. base::Process and FATAL logging, to cause
// coverage information to be stored even when performing an "immediate" exit
// (or triggering a debug crash), where the automatic at-exit writer will not
// be invoked.
// This call is thread-safe, and will write profiling data at-most-once.
void WriteClangCoverageProfile();

}  // namespace base

#endif  // BASE_TEST_CLANG_COVERAGE_H_
