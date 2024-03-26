// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_shared_memory.h"

#include "base/base_switches.h"
#include "base/command_line.h"
#include "base/metrics/persistent_histogram_allocator.h"
#include "base/process/launch.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
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
namespace {

constexpr size_t kArbitrarySize = 64 << 10;

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
constexpr GlobalDescriptors::Key kArbitraryDescriptorKey = 42;
#endif

}  // namespace

TEST(HistogramSharedMemoryTest, Create) {
  UnsafeSharedMemoryRegion region;

  constexpr int kProcessId = 1234;
  constexpr int kProcessType = 5678;
  constexpr char kProcessName[] = "TestProcess";

  auto shared_memory = HistogramSharedMemory::Create(
      kProcessId, {kProcessType, kProcessName, kArbitrarySize});

  ASSERT_TRUE(shared_memory.has_value());

  ASSERT_TRUE(shared_memory->region.IsValid());
  EXPECT_EQ(kArbitrarySize, shared_memory->region.GetSize());

  ASSERT_TRUE(shared_memory->allocator);
  EXPECT_EQ(kArbitrarySize, shared_memory->allocator->size());
}

TEST(HistogramSharedMemoryTest, PassSharedMemoryRegion_Disabled) {
  // Ensure the feature is disabled.
  test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(kPassHistogramSharedMemoryOnLaunch);

  // Create a shared memory region to pass.
  auto memory = UnsafeSharedMemoryRegion::Create(kArbitrarySize);
  ASSERT_TRUE(memory.IsValid());

  // Initialize the command line and launch options.
  CommandLine command_line = GetMultiProcessTestChildBaseCommandLine();
  command_line.AppendSwitchASCII("type", "test-child");
  LaunchOptions launch_options;

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  ScopedFD descriptor_to_share;
#endif

  // Update the launch parameters.
  HistogramSharedMemory::AddToLaunchParameters(memory.Duplicate(),
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
                                               kArbitraryDescriptorKey,
                                               descriptor_to_share,
#endif  // BUILDFLAG(IS_POSIX)
                                               &command_line, &launch_options);

  // The metrics shared memory handle should NOT be added to the command line.
  EXPECT_FALSE(command_line.HasSwitch(switches::kMetricsSharedMemoryHandle));
}

MULTIPROCESS_TEST_MAIN(InitFromLaunchParameters) {
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

  EXPECT_FALSE(GlobalHistogramAllocator::Get());
  // Simulate launching with the serialized parameters.

  HistogramSharedMemory::InitFromLaunchParameters(
      *CommandLine::ForCurrentProcess());
  EXPECT_TRUE(GlobalHistogramAllocator::Get());
  return 0;
}

#if !BUILDFLAG(IS_IOS)
using HistogramSharedMemoryTest = ::testing::TestWithParam<bool>;

INSTANTIATE_TEST_SUITE_P(All,
                         HistogramSharedMemoryTest,
                         ::testing::Values(/*launch_options.elevated=*/false
#if BUILDFLAG(IS_WIN)
                                           ,
                                           /*launch_options.elevated=*/true
#endif
                                           ));

TEST_P(HistogramSharedMemoryTest, PassSharedMemoryRegion_Enabled) {
  // Ensure the feature is enabled.
  test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(kPassHistogramSharedMemoryOnLaunch);

  // Create a shared memory region to pass.
  auto memory = UnsafeSharedMemoryRegion::Create(kArbitrarySize);
  ASSERT_TRUE(memory.IsValid());

  // Initialize the command line and launch options.
  CommandLine command_line = GetMultiProcessTestChildBaseCommandLine();
  command_line.AppendSwitchASCII("type", "test-child");
  LaunchOptions launch_options;

  // On windows, check both the elevated and non-elevated launches.
#if BUILDFLAG(IS_WIN)
  launch_options.start_hidden = true;
  launch_options.elevated = GetParam();
#elif BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  ScopedFD descriptor_to_share;
#endif

  // Update the launch parameters.
  HistogramSharedMemory::AddToLaunchParameters(memory.Duplicate(),
#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
                                               kArbitraryDescriptorKey,
                                               descriptor_to_share,
#endif
                                               &command_line, &launch_options);

  // The metrics shared memory handle should be added to the command line.
  ASSERT_TRUE(command_line.HasSwitch(switches::kMetricsSharedMemoryHandle));
  SCOPED_TRACE(
      command_line.GetSwitchValueASCII(switches::kMetricsSharedMemoryHandle));

#if BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)
  // On posix, AddToLaunchParameters() ignores the launch options and instead
  // returns the descriptor to be shared. This is because the browser child
  // launcher helper manages a separate list of files to share via the zygote,
  // if available. If, like in this test scenario, there's ultimately no zygote
  // to use, launch helper updates the launch options to share the descriptor
  // mapping relative to a base descriptor.
  launch_options.fds_to_remap.emplace_back(descriptor_to_share.get(),
                                           kArbitraryDescriptorKey);
  //  GlobalDescriptors::GetInstance()->Set(kArbitraryDescriptorKey,
  //  descriptor_to_share);
#if !BUILDFLAG(IS_ANDROID)
  for (auto& pair : launch_options.fds_to_remap) {
    pair.second += base::GlobalDescriptors::kBaseDescriptor;
  }
#endif  // !BUILDFLAG(IS_ANDROID)
#endif  // BUILDFLAG(IS_POSIX) && !BUILDFLAG(IS_APPLE)

  // Launch the child process.
  Process process = SpawnMultiProcessTestChild("InitFromLaunchParameters",
                                               command_line, launch_options);

  // The child process returns non-zero if it could not open the shared memory
  // region based on the launch parameters.
  int exit_code;
  EXPECT_TRUE(WaitForMultiprocessTestChildExit(
      process, TestTimeouts::action_timeout(), &exit_code));
  EXPECT_EQ(0, exit_code);
}
#endif

}  // namespace base
