// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/shared_memory_switch.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/stringprintf.h"
#include "base/test/multiprocess_test.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_timeouts.h"
#include "base/unguessable_token.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/files/platform_file.h"
#include "base/posix/global_descriptors.h"
#endif

namespace base {
namespace shared_memory {
namespace {

constexpr char kSharedMemoryData[] = "shared_memory_data";
constexpr char kSharedMemoryGUID[] = "shared_memory_guid";
constexpr char kIsReadOnly[] = "is_read_only";
constexpr size_t kArbitrarySize = 64 << 10;

#if BUILDFLAG(IS_APPLE)
constexpr MachPortsForRendezvous::key_type kArbitraryRendezvousKey = 'smsh';
#elif BUILDFLAG(IS_POSIX)
constexpr GlobalDescriptors::Key kArbitraryDescriptorKey = 42;
#endif

}  // namespace

MULTIPROCESS_TEST_MAIN(InitFromSwitchValue) {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  CHECK(command_line.HasSwitch(kSharedMemoryData));
  CHECK(command_line.HasSwitch(kSharedMemoryGUID));
  CHECK(command_line.HasSwitch(kIsReadOnly));

  // On POSIX we generally use the descriptor map to look up inherited handles.
  // On most POSIX platforms we have to manually sure the mapping is updated,
  // for the purposes of this test.
  //
  // Note:
  //  - This doesn't apply on Apple platforms (which use Rendezvous Keys)
  //  - On Android the global descriptor table is managed by the launcher
  //    service, so we don't have to manually update the mapping here.
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_ANDROID)
  GlobalDescriptors::GetInstance()->Set(
      kArbitraryDescriptorKey,
      kArbitraryDescriptorKey + GlobalDescriptors::kBaseDescriptor);
#endif
  const std::string shared_memory_data =
      command_line.GetSwitchValueASCII(kSharedMemoryData);
  const bool is_read_only =
      command_line.GetSwitchValueASCII(kIsReadOnly) == "true";
  const std::string guid_string =
      command_line.GetSwitchValueASCII(kSharedMemoryGUID);

  if (is_read_only) {
    auto read_only_region = ReadOnlySharedMemoryRegionFrom(shared_memory_data);
    CHECK(read_only_region.has_value());
    CHECK_EQ(guid_string, read_only_region.value().GetGUID().ToString());
  } else {
    auto unsafe_region = UnsafeSharedMemoryRegionFrom(shared_memory_data);
    CHECK(unsafe_region.has_value());
    CHECK_EQ(guid_string, unsafe_region.value().GetGUID().ToString());
  }

  return 0;
}

// The test suite takes two boolean parameters.
using SharedMemorySwitchTest = ::testing::TestWithParam<std::tuple<bool, bool>>;

// Instantiate tests for all combinations of the two boolean parameters.
INSTANTIATE_TEST_SUITE_P(All,
                         SharedMemorySwitchTest,
                         ::testing::Combine(testing::Bool(), testing::Bool()));

TEST_P(SharedMemorySwitchTest, PassViaSwitch) {
  const bool read_only = std::get<0>(GetParam());
  const bool elevated = std::get<1>(GetParam());

  SCOPED_TRACE(
      base::StringPrintf("read_only=%d; elevated=%d", read_only, elevated));

  // Create a shared memory region(s) to pass.
  auto unsafe_region = UnsafeSharedMemoryRegion::Create(kArbitrarySize);
  auto read_only_region = ReadOnlySharedMemoryRegion::Create(kArbitrarySize);
  ASSERT_TRUE(unsafe_region.IsValid());
  ASSERT_TRUE(read_only_region.IsValid());

  // Initialize the command line and launch options.
  CommandLine command_line = GetMultiProcessTestChildBaseCommandLine();
  command_line.AppendSwitchASCII(
      kSharedMemoryGUID,
      (read_only ? read_only_region.region.GetGUID() : unsafe_region.GetGUID())
          .ToString());
  command_line.AppendSwitchASCII(kIsReadOnly, read_only ? "true" : "false");
  LaunchOptions launch_options;

  // On windows, check both the elevated and non-elevated launches.
#if BUILDFLAG(IS_WIN)
  launch_options.start_hidden = true;
  launch_options.elevated = elevated;
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  ScopedFD descriptor_to_share;
#endif

  // Update the launch parameters.
  if (read_only) {
    AddToLaunchParameters(kSharedMemoryData,
                          read_only_region.region.Duplicate(),
#if BUILDFLAG(IS_APPLE)
                          kArbitraryRendezvousKey,
#elif BUILDFLAG(IS_POSIX)
                          kArbitraryDescriptorKey, descriptor_to_share,
#endif
                          &command_line, &launch_options);
  } else {
    AddToLaunchParameters(kSharedMemoryData, unsafe_region.Duplicate(),
#if BUILDFLAG(IS_APPLE)
                          kArbitraryRendezvousKey,
#elif BUILDFLAG(IS_POSIX)
                          kArbitraryDescriptorKey, descriptor_to_share,
#endif
                          &command_line, &launch_options);
  }

  // The metrics shared memory handle should be added to the command line.
  ASSERT_TRUE(command_line.HasSwitch(kSharedMemoryData));
  SCOPED_TRACE(command_line.GetSwitchValueASCII(kSharedMemoryData));

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  // On posix, AddToLaunchParameters() ignores the launch options and instead
  // returns the descriptor to be shared. This is because the browser child
  // launcher helper manages a separate list of files to share via the zygote,
  // if available. If, like in this test scenario, there's ultimately no zygote
  // to use, launch helper updates the launch options to share the descriptor
  // mapping relative to a base descriptor.
  launch_options.fds_to_remap.emplace_back(descriptor_to_share.get(),
                                           kArbitraryDescriptorKey);
#if !BUILDFLAG(IS_ANDROID)
  for (auto& pair : launch_options.fds_to_remap) {
    pair.second += base::GlobalDescriptors::kBaseDescriptor;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)

  // Launch the child process.
  Process process = SpawnMultiProcessTestChild("InitFromSwitchValue",
                                               command_line, launch_options);

  // The child process returns non-zero if it could not open the shared memory
  // region based on the launch parameters.
  int exit_code;
  EXPECT_TRUE(WaitForMultiprocessTestChildExit(
      process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(0, exit_code);
}

}  // namespace shared_memory
}  // namespace base
