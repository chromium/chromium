// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/arc/test/arc_util_test_support.h"

#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/path_service.h"
#include "base/process/process_metrics.h"
#include "base/threading/thread_restrictions.h"

namespace arc {

namespace {

constexpr char kAvailabilityOfficiallySupported[] = "officially-supported";
constexpr char kAlwaysStartWithNoPlayStore[] =
    "always-start-with-no-play-store";

}  // namespace

void SetArcAlwaysStartWithoutPlayStoreForTesting() {
  base::CommandLine::ForCurrentProcess()->AppendSwitchASCII(
      ash::switches::kArcStartMode, kAlwaysStartWithNoPlayStore);
}

void SetArcAvailableCommandLineForTesting(base::CommandLine* command_line) {
  command_line->AppendSwitchASCII(ash::switches::kArcAvailability,
                                  kAvailabilityOfficiallySupported);
}

bool GetSystemMemoryInfoForTesting(const std::string& file_name,
                                   base::SystemMemoryInfoKB* mem_info) {
  base::FilePath base_path;
  base::PathService::Get(base::DIR_SRC_TEST_DATA_ROOT, &base_path);
  const base::FilePath test_path = base_path.Append("ash")
                                       .Append("components")
                                       .Append("arc")
                                       .Append("test")
                                       .Append("data")
                                       .Append("mem_profile")
                                       .Append(file_name);
  base::ScopedAllowBlockingForTesting allowBlocking;
  std::string mem_info_data;
  return base::ReadFileToString(test_path, &mem_info_data) &&
         base::ParseProcMeminfo(mem_info_data, mem_info);
}

}  // namespace arc
