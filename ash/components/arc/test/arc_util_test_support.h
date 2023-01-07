// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_ARC_TEST_ARC_UTIL_TEST_SUPPORT_H_
#define ASH_COMPONENTS_ARC_TEST_ARC_UTIL_TEST_SUPPORT_H_

#include <string>

namespace base {
class CommandLine;
struct SystemMemoryInfoKB;
}  // namespace base

namespace arc {

// Enables to always start ARC without Play Store for testing, by appending the
// command line flag.
void SetArcAlwaysStartWithoutPlayStoreForTesting();

// For testing ARC in browser tests, this function should be called in
// SetUpCommandLine(), and its argument should be passed to this function.
// Also, in unittests, this can be called in SetUp() with
// base::CommandLine::ForCurrentProcess().
// |command_line| must not be nullptr.
void SetArcAvailableCommandLineForTesting(base::CommandLine* command_line);

// Gets a system memory profile based on file name.
bool GetSystemMemoryInfoForTesting(const std::string& file_name,
                                   base::SystemMemoryInfoKB* mem_info);

}  // namespace arc

#endif  // ASH_COMPONENTS_ARC_TEST_ARC_UTIL_TEST_SUPPORT_H_
