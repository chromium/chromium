// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/process/process_metrics.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/functional/bind.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/process/launch.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/gtest_util.h"
#include "base/test/multiprocess_test.h"
#include "base/test/test_timeouts.h"
#include "base/threading/thread.h"
#include "base/types/expected.h"
#include "build/blink_buildflags.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if BUILDFLAG(IS_APPLE)
#include <sys/mman.h>
#endif

#if BUILDFLAG(IS_MAC)
#include <mach/mach.h>

#include "base/apple/mach_logging.h"
#include "base/apple/mach_port_rendezvous.h"
#include "base/apple/scoped_mach_port.h"
#include "base/process/port_provider_mac.h"
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||      \
    BUILDFLAG(IS_CHROMEOS_LACROS) || BUILDFLAG(IS_WIN) || \
    BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_FUCHSIA) || BUILDFLAG(IS_APPLE)
#define ENABLE_CPU_TESTS 1
#else
#define ENABLE_CPU_TESTS 0
#endif

namespace base::debug {

namespace {

using base::test::ErrorIs;
using base::test::ValueIs;
using ::testing::_;
using ::testing::AssertionFailure;
using ::testing::AssertionResult;
using ::testing::AssertionSuccess;
using ::testing::Ge;

#if ENABLE_CPU_TESTS

void BusyWork(std::vector<std::string>* vec) {
  int64_t test_value = 0;
  for (int i = 0; i < 100000; ++i) {
    ++test_value;
    vec->push_back(NumberToString(test_value));
  }
}

// Tests that GetCumulativeCPUUsage() returns a valid result that's equal to or
// greater than `prev_cpu_usage`, and returns the result. If
// GetCumulativeCPUUsage() returns an error, records a failed expectation and
// returns an empty TimeDelta so that each test doesn't need to check for
// nullopt repeatedly.
TimeDelta TestCumulativeCPU(ProcessMetrics* metrics, TimeDelta prev_cpu_usage) {
  const base::expected<TimeDelta, ProcessCPUUsageError> current_cpu_usage =
      metrics->GetCumulativeCPUUsage();
  EXPECT_THAT(current_cpu_usage, ValueIs(Ge(prev_cpu_usage)));
  EXPECT_THAT(metrics->GetPlatformIndependentCPUUsage(), ValueIs(Ge(0.0)));
  return current_cpu_usage.value_or(TimeDelta());
}

#endif  // ENABLE_CPU_TESTS

// Helper to deal with Mac process launching complexity. On other platforms this
// is just a thin wrapper around SpawnMultiProcessTestChild.
class TestChildLauncher {
 public:
  TestChildLauncher() = default;
  ~TestChildLauncher() = default;

  TestChildLauncher(const TestChildLauncher&) = delete;
  TestChildLauncher& operator=(const TestChildLauncher&) = delete;

  // Returns a reference to the command line for the child process. This can be
  // used to add extra parameters before calling SpawnChildProcess().
  CommandLine& command_line() { return command_line_; }

  // Returns a reference to the child process object, which will be invalid
  // until SpawnChildProcess() is called.
  Process& child_process() { return child_process_; }

  // Spawns a multiprocess test child to execute the function `procname`.
  AssertionResult SpawnChildProcess(const std::string& procname);

  // Returns a ProcessMetrics object for the child process created by
  // SpawnChildProcess().
  std::unique_ptr<ProcessMetrics> CreateChildProcessMetrics();

  // Terminates the child process created by SpawnChildProcess(). Returns true
  // if the process successfully terminates within the allowed time.
  bool TerminateChildProcess();

  // Called from the child process to send data back to the parent if needed.
  static void NotifyParent();

 private:
  CommandLine command_line_ = GetMultiProcessTestChildBaseCommandLine();
  Process child_process_;

#if BUILDFLAG(IS_MAC)
  class TestChildPortProvider;
  std::unique_ptr<TestChildPortProvider> port_provider_;
#endif
};

#if BUILDFLAG(IS_MAC)

// Adapted from base/apple/mach_port_rendezvous_unittest.cc and
// https://mw.foldr.org/posts/computers/macosx/task-info-fun-with-mach/

constexpr MachPortsForRendezvous::key_type kTestChildRendezvousKey = 'test';

// A PortProvider that tracks child processes spawned by TestChildLauncher.
class TestChildLauncher::TestChildPortProvider final : public PortProvider {
 public:
  TestChildPortProvider(ProcessHandle handle, apple::ScopedMachSendRight port)
      : handle_(handle), port_(std::move(port)) {}

  ~TestChildPortProvider() final = default;

  TestChildPortProvider(const TestChildPortProvider&) = delete;
  TestChildPortProvider& operator=(const TestChildPortProvider&) = delete;

  mach_port_t TaskForHandle(ProcessHandle process_handle) const final {
    return process_handle == handle_ ? port_.get() : MACH_PORT_NULL;
  }

 private:
  ProcessHandle handle_;
  apple::ScopedMachSendRight port_;
};

AssertionResult TestChildLauncher::SpawnChildProcess(
    const std::string& procname) {
  // Allocate a port for the parent to receive details from the child process.
  apple::ScopedMachReceiveRight receive_port;
  if (!apple::CreateMachPort(&receive_port, nullptr)) {
    return AssertionFailure() << "Failed to allocate receive port";
  }

  // Pass the sending end of the port to the child.
  LaunchOptions options = LaunchOptionsForTest();
  options.mach_ports_for_rendezvous.emplace(
      kTestChildRendezvousKey,
      MachRendezvousPort(receive_port.get(), MACH_MSG_TYPE_MAKE_SEND));
  child_process_ =
      SpawnMultiProcessTestChild(procname, command_line_, std::move(options));
  if (!child_process_.IsValid()) {
    return AssertionFailure() << "Failed to launch child process.";
  }

  // Wait for the child to send back its mach_task_self().
  struct : mach_msg_base_t {
    mach_msg_port_descriptor_t task_port;
    mach_msg_trailer_t trailer;
  } msg{};
  kern_return_t kr =
      mach_msg(&msg.header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof(msg),
               receive_port.get(),
               TestTimeouts::action_timeout().InMilliseconds(), MACH_PORT_NULL);
  if (kr != KERN_SUCCESS) {
    return AssertionFailure()
           << "Failed to read mach_task_self from child process: "
           << mach_error_string(kr);
  }
  port_provider_ = std::make_unique<TestChildPortProvider>(
      child_process_.Handle(), apple::ScopedMachSendRight(msg.task_port.name));
  return AssertionSuccess();
}

std::unique_ptr<ProcessMetrics> TestChildLauncher::CreateChildProcessMetrics() {
#if BUILDFLAG(IS_MAC)
  return ProcessMetrics::CreateProcessMetrics(child_process_.Handle(),
                                              port_provider_.get());
#else
  return ProcessMetrics::CreateProcessMetrics(child_process_.Handle());
#endif
}

bool TestChildLauncher::TerminateChildProcess() {
  return TerminateMultiProcessTestChild(child_process_, /*exit_code=*/0,
                                        /*wait=*/true);
}

// static
void TestChildLauncher::NotifyParent() {
  auto* client = MachPortRendezvousClient::GetInstance();
  ASSERT_TRUE(client);
  apple::ScopedMachSendRight send_port =
      client->TakeSendRight(kTestChildRendezvousKey);
  ASSERT_TRUE(send_port.is_valid());

  // Send mach_task_self to the parent process so that it can use the port to
  // create ProcessMetrics.
  struct : mach_msg_base_t {
    mach_msg_port_descriptor_t task_port;
  } msg{};
  msg.header.msgh_bits =
      MACH_MSGH_BITS_REMOTE(MACH_MSG_TYPE_COPY_SEND) | MACH_MSGH_BITS_COMPLEX;
  msg.header.msgh_remote_port = send_port.get();
  msg.header.msgh_size = sizeof(msg);
  msg.body.msgh_descriptor_count = 1;
  msg.task_port.name = mach_task_self();
  msg.task_port.disposition = MACH_MSG_TYPE_COPY_SEND;
  msg.task_port.type = MACH_MSG_PORT_DESCRIPTOR;
  kern_return_t kr =
      mach_msg(&msg.header, MACH_SEND_MSG, msg.header.msgh_size, 0,
               MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
  MACH_CHECK(kr == KERN_SUCCESS, kr);
}

#else

AssertionResult TestChildLauncher::SpawnChildProcess(
    const std::string& procname) {
  child_process_ = SpawnMultiProcessTestChild(procname, command_line_,
                                              LaunchOptionsForTest());
  return child_process_.IsValid()
             ? AssertionSuccess()
             : AssertionFailure() << "Failed to launch child process.";
}

std::unique_ptr<ProcessMetrics> TestChildLauncher::CreateChildProcessMetrics() {
  return ProcessMetrics::CreateProcessMetrics(child_process_.Handle());
}

bool TestChildLauncher::TerminateChildProcess() {
  [[maybe_unused]] const ProcessHandle child_handle = child_process_.Handle();
  if (!TerminateMultiProcessTestChild(child_process_, /*exit_code=*/0,
                                      /*wait=*/true)) {
    return false;
  }
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  // After the process exits, ProcessMetrics races to read /proc/<pid>/stat
  // before it's deleted. Wait until it's definitely gone.
  const auto stat_path = FilePath(FILE_PATH_LITERAL("/proc"))
                             .AppendASCII(NumberToString(child_handle))
                             .Append(FILE_PATH_LITERAL("stat"));

  while (PathExists(stat_path)) {
    PlatformThread::Sleep(TestTimeouts::tiny_timeout());
  }
#endif
  return true;
}

// static
void TestChildLauncher::NotifyParent() {
  // Do nothing.
}

#endif  // BUILDFLAG(IS_MAC)

}  // namespace

// Tests for SystemMetrics.
// Exists as a class so it can be a friend of SystemMetrics.
class SystemMetricsTest : public testing::Test {
 public:
  SystemMetricsTest() = default;

  SystemMetricsTest(const SystemMetricsTest&) = delete;
  SystemMetricsTest& operator=(const SystemMetricsTest&) = delete;
};

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
TEST_F(SystemMetricsTest, IsValidDiskName) {
  const char invalid_input1[] = "";
  const char invalid_input2[] = "s";
  const char invalid_input3[] = "sdz+";
  const char invalid_input4[] = "hda0";
  const char invalid_input5[] = "mmcbl";
  const char invalid_input6[] = "mmcblka";
  const char invalid_input7[] = "mmcblkb";
  const char invalid_input8[] = "mmmblk0";

  EXPECT_FALSE(IsValidDiskName(invalid_input1));
  EXPECT_FALSE(IsValidDiskName(invalid_input2));
  EXPECT_FALSE(IsValidDiskName(invalid_input3));
  EXPECT_FALSE(IsValidDiskName(invalid_input4));
  EXPECT_FALSE(IsValidDiskName(invalid_input5));
  EXPECT_FALSE(IsValidDiskName(invalid_input6));
  EXPECT_FALSE(IsValidDiskName(invalid_input7));
  EXPECT_FALSE(IsValidDiskName(invalid_input8));

  const char valid_input1[] = "sda";
  const char valid_input2[] = "sdaaaa";
  const char valid_input3[] = "hdz";
  const char valid_input4[] = "mmcblk0";
  const char valid_input5[] = "mmcblk999";

  EXPECT_TRUE(IsValidDiskName(valid_input1));
  EXPECT_TRUE(IsValidDiskName(valid_input2));
  EXPECT_TRUE(IsValidDiskName(valid_input3));
  EXPECT_TRUE(IsValidDiskName(valid_input4));
  EXPECT_TRUE(IsValidDiskName(valid_input5));
}

TEST_F(SystemMetricsTest, ParseMeminfo) {
  SystemMemoryInfoKB meminfo;
  const char invalid_input1[] = "abc";
  const char invalid_input2[] = "MemTotal:";
  // Partial file with no MemTotal
  const char invalid_input3[] =
      "MemFree:         3913968 kB\n"
      "Buffers:         2348340 kB\n"
      "Cached:         49071596 kB\n"
      "SwapCached:           12 kB\n"
      "Active:         36393900 kB\n"
      "Inactive:       21221496 kB\n"
      "Active(anon):    5674352 kB\n"
      "Inactive(anon):   633992 kB\n";
  EXPECT_FALSE(ParseProcMeminfo(invalid_input1, &meminfo));
  EXPECT_FALSE(ParseProcMeminfo(invalid_input2, &meminfo));
  EXPECT_FALSE(ParseProcMeminfo(invalid_input3, &meminfo));

  const char valid_input1[] =
      "MemTotal:        3981504 kB\n"
      "MemFree:          140764 kB\n"
      "MemAvailable:     535413 kB\n"
      "Buffers:          116480 kB\n"
      "Cached:           406160 kB\n"
      "SwapCached:        21304 kB\n"
      "Active:          3152040 kB\n"
      "Inactive:         472856 kB\n"
      "Active(anon):    2972352 kB\n"
      "Inactive(anon):   270108 kB\n"
      "Active(file):     179688 kB\n"
      "Inactive(file):   202748 kB\n"
      "Unevictable:           0 kB\n"
      "Mlocked:               0 kB\n"
      "SwapTotal:       5832280 kB\n"
      "SwapFree:        3672368 kB\n"
      "Dirty:               184 kB\n"
      "Writeback:             0 kB\n"
      "AnonPages:       3101224 kB\n"
      "Mapped:           142296 kB\n"
      "Shmem:            140204 kB\n"
      "Slab:              54212 kB\n"
      "SReclaimable:      30936 kB\n"
      "SUnreclaim:        23276 kB\n"
      "KernelStack:        2464 kB\n"
      "PageTables:        24812 kB\n"
      "NFS_Unstable:          0 kB\n"
      "Bounce:                0 kB\n"
      "WritebackTmp:          0 kB\n"
      "CommitLimit:     7823032 kB\n"
      "Committed_AS:    7973536 kB\n"
      "VmallocTotal:   34359738367 kB\n"
      "VmallocUsed:      375940 kB\n"
      "VmallocChunk:   34359361127 kB\n"
      "DirectMap4k:       72448 kB\n"
      "DirectMap2M:     4061184 kB\n";
  // output from a much older kernel where the Active and Inactive aren't
  // broken down into anon and file and Huge Pages are enabled
  const char valid_input2[] =
      "MemTotal:       255908 kB\n"
      "MemFree:         69936 kB\n"
      "Buffers:         15812 kB\n"
      "Cached:         115124 kB\n"
      "SwapCached:          0 kB\n"
      "Active:          92700 kB\n"
      "Inactive:        63792 kB\n"
      "HighTotal:           0 kB\n"
      "HighFree:            0 kB\n"
      "LowTotal:       255908 kB\n"
      "LowFree:         69936 kB\n"
      "SwapTotal:      524280 kB\n"
      "SwapFree:       524200 kB\n"
      "Dirty:               4 kB\n"
      "Writeback:           0 kB\n"
      "Mapped:          42236 kB\n"
      "Slab:            25912 kB\n"
      "Committed_AS:   118680 kB\n"
      "PageTables:       1236 kB\n"
      "VmallocTotal:  3874808 kB\n"
      "VmallocUsed:      1416 kB\n"
      "VmallocChunk:  3872908 kB\n"
      "HugePages_Total:     0\n"
      "HugePages_Free:      0\n"
      "Hugepagesize:     4096 kB\n";

  EXPECT_TRUE(ParseProcMeminfo(valid_input1, &meminfo));
  EXPECT_EQ(meminfo.total, 3981504);
  EXPECT_EQ(meminfo.free, 140764);
  EXPECT_EQ(meminfo.available, 535413);
  EXPECT_EQ(meminfo.buffers, 116480);
  EXPECT_EQ(meminfo.cached, 406160);
  EXPECT_EQ(meminfo.active_anon, 2972352);
  EXPECT_EQ(meminfo.active_file, 179688);
  EXPECT_EQ(meminfo.inactive_anon, 270108);
  EXPECT_EQ(meminfo.inactive_file, 202748);
  EXPECT_EQ(meminfo.swap_total, 5832280);
  EXPECT_EQ(meminfo.swap_free, 3672368);
  EXPECT_EQ(meminfo.dirty, 184);
  EXPECT_EQ(meminfo.reclaimable, 30936);
#if BUILDFLAG(IS_CHROMEOS)
  EXPECT_EQ(meminfo.shmem, 140204);
  EXPECT_EQ(meminfo.slab, 54212);
#endif
  EXPECT_EQ(355725u,
            base::SysInfo::AmountOfAvailablePhysicalMemory(meminfo) / 1024);
  // Simulate as if there is no MemAvailable.
  meminfo.available = 0;
  EXPECT_EQ(374448u,
            base::SysInfo::AmountOfAvailablePhysicalMemory(meminfo) / 1024);
  meminfo = {};
  EXPECT_TRUE(ParseProcMeminfo(valid_input2, &meminfo));
  EXPECT_EQ(meminfo.total, 255908);
  EXPECT_EQ(meminfo.free, 69936);
  EXPECT_EQ(meminfo.available, 0);
  EXPECT_EQ(meminfo.buffers, 15812);
  EXPECT_EQ(meminfo.cached, 115124);
  EXPECT_EQ(meminfo.swap_total, 524280);
  EXPECT_EQ(meminfo.swap_free, 524200);
  EXPECT_EQ(meminfo.dirty, 4);
  EXPECT_EQ(69936u,
            base::SysInfo::AmountOfAvailablePhysicalMemory(meminfo) / 1024);

  // output from a system with a large page cache, to catch arithmetic errors
  // that incorrectly assume free + buffers + cached <= total. (Copied from
  // ash/components/arc/test/data/mem_profile/16G.)
  const char large_cache_input[] =
      "MemTotal:       18025572 kB\n"
      "MemFree:        13150176 kB\n"
      "MemAvailable:   15447672 kB\n"
      "Buffers:         1524852 kB\n"
      "Cached:         12645260 kB\n"
      "SwapCached:            0 kB\n"
      "Active:          2572904 kB\n"
      "Inactive:        1064976 kB\n"
      "Active(anon):    1047836 kB\n"
      "Inactive(anon):    11736 kB\n"
      "Active(file):    1525068 kB\n"
      "Inactive(file):  1053240 kB\n"
      "Unevictable:      611904 kB\n"
      "Mlocked:           32884 kB\n"
      "SwapTotal:      11756208 kB\n"
      "SwapFree:       11756208 kB\n"
      "Dirty:              4152 kB\n"
      "Writeback:             0 kB\n"
      "AnonPages:       1079660 kB\n"
      "Mapped:           782152 kB\n"
      "Shmem:            591820 kB\n"
      "Slab:             366104 kB\n"
      "SReclaimable:     254356 kB\n"
      "SUnreclaim:       111748 kB\n"
      "KernelStack:       22652 kB\n"
      "PageTables:        41540 kB\n"
      "NFS_Unstable:          0 kB\n"
      "Bounce:                0 kB\n"
      "WritebackTmp:          0 kB\n"
      "CommitLimit:    15768992 kB\n"
      "Committed_AS:   36120244 kB\n"
      "VmallocTotal:   34359738367 kB\n"
      "VmallocUsed:           0 kB\n"
      "VmallocChunk:          0 kB\n"
      "Percpu:             3328 kB\n"
      "AnonHugePages:     32768 kB\n"
      "ShmemHugePages:        0 kB\n"
      "ShmemPmdMapped:        0 kB\n"
      "DirectMap4k:      293036 kB\n"
      "DirectMap2M:     6918144 kB\n"
      "DirectMap1G:     2097152 kB\n";

  meminfo = {};
  EXPECT_TRUE(ParseProcMeminfo(large_cache_input, &meminfo));
  EXPECT_EQ(meminfo.total, 18025572);
  EXPECT_EQ(meminfo.free, 13150176);
  EXPECT_EQ(meminfo.buffers, 1524852);
  EXPECT_EQ(meminfo.cached, 12645260);
  EXPECT_EQ(GetSystemCommitChargeFromMeminfo(meminfo), 0u);
}

TEST_F(SystemMetricsTest, ParseVmstat) {
  VmStatInfo vmstat;
  // Part of vmstat from a 4.19 kernel.
  const char valid_input1[] =
      "pgpgin 2358216\n"
      "pgpgout 296072\n"
      "pswpin 345219\n"
      "pswpout 2605828\n"
      "pgalloc_dma32 8380235\n"
      "pgalloc_normal 3384525\n"
      "pgalloc_movable 0\n"
      "allocstall_dma32 0\n"
      "allocstall_normal 2028\n"
      "allocstall_movable 32559\n"
      "pgskip_dma32 0\n"
      "pgskip_normal 0\n"
      "pgskip_movable 0\n"
      "pgfree 11802722\n"
      "pgactivate 894917\n"
      "pgdeactivate 3255711\n"
      "pglazyfree 48\n"
      "pgfault 10043657\n"
      "pgmajfault 358901\n"
      "pgmajfault_s 2100\n"
      "pgmajfault_a 343211\n"
      "pgmajfault_f 13590\n"
      "pglazyfreed 0\n"
      "pgrefill 3429488\n"
      "pgsteal_kswapd 1466893\n"
      "pgsteal_direct 1771759\n"
      "pgscan_kswapd 1907332\n"
      "pgscan_direct 2118930\n"
      "pgscan_direct_throttle 154\n"
      "pginodesteal 3176\n"
      "slabs_scanned 293804\n"
      "kswapd_inodesteal 16753\n"
      "kswapd_low_wmark_hit_quickly 10\n"
      "kswapd_high_wmark_hit_quickly 423\n"
      "pageoutrun 441\n"
      "pgrotated 1636\n"
      "drop_pagecache 0\n"
      "drop_slab 0\n"
      "oom_kill 18\n";
  const char valid_input2[] =
      "pgpgin 2606135\n"
      "pgpgout 1359128\n"
      "pswpin 899959\n"
      "pswpout 19761244\n"
      "pgalloc_dma 31\n"
      "pgalloc_dma32 18139339\n"
      "pgalloc_normal 44085950\n"
      "pgalloc_movable 0\n"
      "allocstall_dma 0\n"
      "allocstall_dma32 0\n"
      "allocstall_normal 18881\n"
      "allocstall_movable 169527\n"
      "pgskip_dma 0\n"
      "pgskip_dma32 0\n"
      "pgskip_normal 0\n"
      "pgskip_movable 0\n"
      "pgfree 63060999\n"
      "pgactivate 1703494\n"
      "pgdeactivate 20537803\n"
      "pglazyfree 163\n"
      "pgfault 45201169\n"
      "pgmajfault 609626\n"
      "pgmajfault_s 7488\n"
      "pgmajfault_a 591793\n"
      "pgmajfault_f 10345\n"
      "pglazyfreed 0\n"
      "pgrefill 20673453\n"
      "pgsteal_kswapd 11802772\n"
      "pgsteal_direct 8618160\n"
      "pgscan_kswapd 12640517\n"
      "pgscan_direct 9092230\n"
      "pgscan_direct_throttle 638\n"
      "pginodesteal 1716\n"
      "slabs_scanned 2594642\n"
      "kswapd_inodesteal 67358\n"
      "kswapd_low_wmark_hit_quickly 52\n"
      "kswapd_high_wmark_hit_quickly 11\n"
      "pageoutrun 83\n"
      "pgrotated 977\n"
      "drop_pagecache 1\n"
      "drop_slab 1\n"
      "oom_kill 1\n"
      "pgmigrate_success 3202\n"
      "pgmigrate_fail 795\n";
  const char valid_input3[] =
      "pswpin 12\n"
      "pswpout 901\n"
      "pgmajfault 18881\n";
  EXPECT_TRUE(ParseProcVmstat(valid_input1, &vmstat));
  EXPECT_EQ(345219LU, vmstat.pswpin);
  EXPECT_EQ(2605828LU, vmstat.pswpout);
  EXPECT_EQ(358901LU, vmstat.pgmajfault);
  EXPECT_EQ(18LU, vmstat.oom_kill);
  EXPECT_TRUE(ParseProcVmstat(valid_input2, &vmstat));
  EXPECT_EQ(899959LU, vmstat.pswpin);
  EXPECT_EQ(19761244LU, vmstat.pswpout);
  EXPECT_EQ(609626LU, vmstat.pgmajfault);
  EXPECT_EQ(1LU, vmstat.oom_kill);
  EXPECT_TRUE(ParseProcVmstat(valid_input3, &vmstat));
  EXPECT_EQ(12LU, vmstat.pswpin);
  EXPECT_EQ(901LU, vmstat.pswpout);
  EXPECT_EQ(18881LU, vmstat.pgmajfault);
  EXPECT_EQ(0LU, vmstat.oom_kill);

  const char missing_pgmajfault_input[] =
      "pswpin 12\n"
      "pswpout 901\n";
  EXPECT_FALSE(ParseProcVmstat(missing_pgmajfault_input, &vmstat));
  const char empty_input[] = "";
  EXPECT_FALSE(ParseProcVmstat(empty_input, &vmstat));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

#if ENABLE_CPU_TESTS
// Test that ProcessMetrics::GetPlatformIndependentCPUUsage() doesn't return
// negative values when the number of threads running on the process decreases
// between two successive calls to it.
TEST_F(SystemMetricsTest, TestNoNegativeCpuUsage) {
  std::unique_ptr<ProcessMetrics> metrics =
      ProcessMetrics::CreateCurrentProcessMetrics();

  EXPECT_THAT(metrics->GetPlatformIndependentCPUUsage(), ValueIs(Ge(0.0)));

  Thread thread1("thread1");
  Thread thread2("thread2");
  Thread thread3("thread3");

  thread1.StartAndWaitForTesting();
  thread2.StartAndWaitForTesting();
  thread3.StartAndWaitForTesting();

  ASSERT_TRUE(thread1.IsRunning());
  ASSERT_TRUE(thread2.IsRunning());
  ASSERT_TRUE(thread3.IsRunning());

  std::vector<std::string> vec1;
  std::vector<std::string> vec2;
  std::vector<std::string> vec3;

  thread1.task_runner()->PostTask(FROM_HERE, BindOnce(&BusyWork, &vec1));
  thread2.task_runner()->PostTask(FROM_HERE, BindOnce(&BusyWork, &vec2));
  thread3.task_runner()->PostTask(FROM_HERE, BindOnce(&BusyWork, &vec3));

  TimeDelta prev_cpu_usage = TestCumulativeCPU(metrics.get(), TimeDelta());

  thread1.Stop();
  prev_cpu_usage = TestCumulativeCPU(metrics.get(), prev_cpu_usage);

  thread2.Stop();
  prev_cpu_usage = TestCumulativeCPU(metrics.get(), prev_cpu_usage);

  thread3.Stop();
  prev_cpu_usage = TestCumulativeCPU(metrics.get(), prev_cpu_usage);
}

#if !BUILDFLAG(IS_APPLE)

// Subprocess to test the child CPU usage.
MULTIPROCESS_TEST_MAIN(CPUUsageChildMain) {
  TestChildLauncher::NotifyParent();
  // Busy wait until terminated.
  while (true) {
    std::vector<std::string> vec;
    BusyWork(&vec);
  }
}

TEST_F(SystemMetricsTest, MeasureChildCpuUsage) {
  TestChildLauncher child_launcher;
  ASSERT_TRUE(child_launcher.SpawnChildProcess("CPUUsageChildMain"));
  std::unique_ptr<ProcessMetrics> metrics =
      child_launcher.CreateChildProcessMetrics();

  const TimeDelta cpu_usage1 = TestCumulativeCPU(metrics.get(), TimeDelta());

  // The child thread does busy work, so it should get some CPU usage. There's a
  // small chance it won't be scheduled during the delay so loop several times.
  const auto abort_time =
      base::TimeTicks::Now() + TestTimeouts::action_max_timeout();
  TimeDelta cpu_usage2;
  while (cpu_usage2.is_zero() && !HasFailure() &&
         base::TimeTicks::Now() < abort_time) {
    PlatformThread::Sleep(TestTimeouts::tiny_timeout());
    cpu_usage2 = TestCumulativeCPU(metrics.get(), cpu_usage1);
  }
  EXPECT_TRUE(cpu_usage2.is_positive());

  ASSERT_TRUE(child_launcher.TerminateChildProcess());

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_FUCHSIA)
  // Windows and Fuchsia return final measurements of a process after it exits.
  TestCumulativeCPU(metrics.get(), cpu_usage2);
#else
  // All other platforms return an error.
  EXPECT_THAT(metrics->GetCumulativeCPUUsage(), ErrorIs(_));
  EXPECT_THAT(metrics->GetPlatformIndependentCPUUsage(), ErrorIs(_));
#endif
}

#endif  // !BUILDFLAG(IS_APPLE)

TEST_F(SystemMetricsTest, InvalidProcessCpuUsage) {
#if BUILDFLAG(IS_MAC)
  std::unique_ptr<ProcessMetrics> metrics =
      ProcessMetrics::CreateProcessMetrics(kNullProcessHandle, nullptr);
#else
  std::unique_ptr<ProcessMetrics> metrics =
      ProcessMetrics::CreateProcessMetrics(kNullProcessHandle);
#endif
  EXPECT_THAT(metrics->GetCumulativeCPUUsage(), ErrorIs(_));
  EXPECT_THAT(metrics->GetPlatformIndependentCPUUsage(), ErrorIs(_));
}

#endif  // ENABLE_CPU_TESTS

#if BUILDFLAG(IS_CHROMEOS)
TEST_F(SystemMetricsTest, ParseZramMmStat) {
  SwapInfo swapinfo;

  const char invalid_input1[] = "aaa";
  const char invalid_input2[] = "1 2 3 4 5 6";
  const char invalid_input3[] = "a 2 3 4 5 6 7";
  EXPECT_FALSE(ParseZramMmStat(invalid_input1, &swapinfo));
  EXPECT_FALSE(ParseZramMmStat(invalid_input2, &swapinfo));
  EXPECT_FALSE(ParseZramMmStat(invalid_input3, &swapinfo));

  const char valid_input1[] =
      "17715200 5008166 566062  0 1225715712  127 183842";
  EXPECT_TRUE(ParseZramMmStat(valid_input1, &swapinfo));
  EXPECT_EQ(17715200ULL, swapinfo.orig_data_size);
  EXPECT_EQ(5008166ULL, swapinfo.compr_data_size);
  EXPECT_EQ(566062ULL, swapinfo.mem_used_total);
}

TEST_F(SystemMetricsTest, ParseZramStat) {
  SwapInfo swapinfo;

  const char invalid_input1[] = "aaa";
  const char invalid_input2[] = "1 2 3 4 5 6 7 8 9 10";
  const char invalid_input3[] = "a 2 3 4 5 6 7 8 9 10 11";
  EXPECT_FALSE(ParseZramStat(invalid_input1, &swapinfo));
  EXPECT_FALSE(ParseZramStat(invalid_input2, &swapinfo));
  EXPECT_FALSE(ParseZramStat(invalid_input3, &swapinfo));

  const char valid_input1[] =
      "299    0    2392    0    1    0    8    0    0    0    0";
  EXPECT_TRUE(ParseZramStat(valid_input1, &swapinfo));
  EXPECT_EQ(299ULL, swapinfo.num_reads);
  EXPECT_EQ(1ULL, swapinfo.num_writes);
}
#endif  // BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) || \
    BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
TEST(SystemMetrics2Test, GetSystemMemoryInfo) {
  SystemMemoryInfoKB info;
  EXPECT_TRUE(GetSystemMemoryInfo(&info));

  // Ensure each field received a value.
  EXPECT_GT(info.total, 0);
#if BUILDFLAG(IS_WIN)
  EXPECT_GT(info.avail_phys, 0);
#else
  EXPECT_GT(info.free, 0);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  EXPECT_GT(info.buffers, 0);
  EXPECT_GT(info.cached, 0);
  EXPECT_GT(info.active_anon + info.inactive_anon, 0);
  EXPECT_GT(info.active_file + info.inactive_file, 0);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

  // All the values should be less than the total amount of memory.
#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_IOS)
  // TODO(crbug.com/40515565): re-enable the following assertion on iOS.
  EXPECT_LT(info.free, info.total);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
  EXPECT_LT(info.buffers, info.total);
  EXPECT_LT(info.cached, info.total);
  EXPECT_LT(info.active_anon, info.total);
  EXPECT_LT(info.inactive_anon, info.total);
  EXPECT_LT(info.active_file, info.total);
  EXPECT_LT(info.inactive_file, info.total);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_APPLE)
  EXPECT_GT(info.file_backed, 0);
#endif

#if BUILDFLAG(IS_CHROMEOS)
  // Chrome OS exposes shmem.
  EXPECT_GT(info.shmem, 0);
  EXPECT_LT(info.shmem, info.total);
#endif
}
#endif  // BUILDFLAG(IS_WIN) || BUILDFLAG(IS_APPLE) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
TEST(ProcessMetricsTest, ParseProcStatCPU) {
  // /proc/self/stat for a process running "top".
  const char kTopStat[] =
      "960 (top) S 16230 960 16230 34818 960 "
      "4202496 471 0 0 0 "
      "12 16 0 0 "  // <- These are the goods.
      "20 0 1 0 121946157 15077376 314 18446744073709551615 4194304 "
      "4246868 140733983044336 18446744073709551615 140244213071219 "
      "0 0 0 138047495 0 0 0 17 1 0 0 0 0 0";
  EXPECT_EQ(12 + 16, ParseProcStatCPU(kTopStat));

  // cat /proc/self/stat on a random other machine I have.
  const char kSelfStat[] =
      "5364 (cat) R 5354 5364 5354 34819 5364 "
      "0 142 0 0 0 "
      "0 0 0 0 "  // <- No CPU, apparently.
      "16 0 1 0 1676099790 2957312 114 4294967295 134512640 134528148 "
      "3221224832 3221224344 3086339742 0 0 0 0 0 0 0 17 0 0 0";

  EXPECT_EQ(0, ParseProcStatCPU(kSelfStat));

  // Some weird long-running process with a weird name that I created for the
  // purposes of this test.
  const char kWeirdNameStat[] =
      "26115 (Hello) You ()))  ) R 24614 26115 24614"
      " 34839 26115 4218880 227 0 0 0 "
      "5186 11 0 0 "
      "20 0 1 0 36933953 4296704 90 18446744073709551615 4194304 4196116 "
      "140735857761568 140735857761160 4195644 0 0 0 0 0 0 0 17 14 0 0 0 0 0 "
      "6295056 6295616 16519168 140735857770710 140735857770737 "
      "140735857770737 140735857774557 0";
  EXPECT_EQ(5186 + 11, ParseProcStatCPU(kWeirdNameStat));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) ||
        // BUILDFLAG(IS_ANDROID)

// Disable on Android because base_unittests runs inside a Dalvik VM that
// starts and stop threads (crbug.com/175563).
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// http://crbug.com/396455
TEST(ProcessMetricsTest, DISABLED_GetNumberOfThreads) {
  const ProcessHandle current = GetCurrentProcessHandle();
  const int64_t initial_threads = GetNumberOfThreads(current);
  ASSERT_GT(initial_threads, 0);
  const int kNumAdditionalThreads = 10;
  {
    std::unique_ptr<Thread> my_threads[kNumAdditionalThreads];
    for (int i = 0; i < kNumAdditionalThreads; ++i) {
      my_threads[i] = std::make_unique<Thread>("GetNumberOfThreadsTest");
      my_threads[i]->Start();
      ASSERT_EQ(GetNumberOfThreads(current), initial_threads + 1 + i);
    }
  }
  // The Thread destructor will stop them.
  ASSERT_EQ(initial_threads, GetNumberOfThreads(current));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
namespace {

// Keep these in sync so the GetChildOpenFdCount test can refer to correct test
// main.
#define ChildMain ChildFdCount
#define ChildMainString "ChildFdCount"

// Command line flag name and file name used for synchronization.
const char kTempDirFlag[] = "temp-dir";

const char kSignalReady[] = "ready";
const char kSignalReadyAck[] = "ready-ack";
const char kSignalOpened[] = "opened";
const char kSignalOpenedAck[] = "opened-ack";
const char kSignalClosed[] = "closed";

const int kChildNumFilesToOpen = 100;

bool SignalEvent(const FilePath& signal_dir, const char* signal_file) {
  File file(signal_dir.AppendASCII(signal_file),
            File::FLAG_CREATE | File::FLAG_WRITE);
  return file.IsValid();
}

// Check whether an event was signaled.
bool CheckEvent(const FilePath& signal_dir, const char* signal_file) {
  File file(signal_dir.AppendASCII(signal_file),
            File::FLAG_OPEN | File::FLAG_READ);
  return file.IsValid();
}

// Busy-wait for an event to be signaled.
void WaitForEvent(const FilePath& signal_dir, const char* signal_file) {
  while (!CheckEvent(signal_dir, signal_file)) {
    PlatformThread::Sleep(Milliseconds(10));
  }
}

// Subprocess to test the number of open file descriptors.
MULTIPROCESS_TEST_MAIN(ChildMain) {
  TestChildLauncher::NotifyParent();

  CommandLine* command_line = CommandLine::ForCurrentProcess();
  const FilePath temp_path = command_line->GetSwitchValuePath(kTempDirFlag);
  CHECK(DirectoryExists(temp_path));

  CHECK(SignalEvent(temp_path, kSignalReady));
  WaitForEvent(temp_path, kSignalReadyAck);

  std::vector<File> files;
  for (int i = 0; i < kChildNumFilesToOpen; ++i) {
    files.emplace_back(temp_path.AppendASCII(StringPrintf("file.%d", i)),
                       File::FLAG_CREATE | File::FLAG_WRITE);
  }

  CHECK(SignalEvent(temp_path, kSignalOpened));
  WaitForEvent(temp_path, kSignalOpenedAck);

  files.clear();

  CHECK(SignalEvent(temp_path, kSignalClosed));

  // Wait to be terminated.
  while (true) {
    PlatformThread::Sleep(Seconds(1));
  }
}

}  // namespace

TEST(ProcessMetricsTest, GetChildOpenFdCount) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath temp_path = temp_dir.GetPath();

  TestChildLauncher child_launcher;
  child_launcher.command_line().AppendSwitchPath(kTempDirFlag, temp_path);
  ASSERT_TRUE(child_launcher.SpawnChildProcess(ChildMainString));

  WaitForEvent(temp_path, kSignalReady);

  std::unique_ptr<ProcessMetrics> metrics =
      child_launcher.CreateChildProcessMetrics();

  const int fd_count = metrics->GetOpenFdCount();
  EXPECT_GE(fd_count, 0);

  ASSERT_TRUE(SignalEvent(temp_path, kSignalReadyAck));
  WaitForEvent(temp_path, kSignalOpened);

  EXPECT_EQ(fd_count + kChildNumFilesToOpen, metrics->GetOpenFdCount());
  ASSERT_TRUE(SignalEvent(temp_path, kSignalOpenedAck));

  WaitForEvent(temp_path, kSignalClosed);

  EXPECT_EQ(fd_count, metrics->GetOpenFdCount());

  ASSERT_TRUE(child_launcher.TerminateChildProcess());
}

TEST(ProcessMetricsTest, GetOpenFdCount) {
  std::unique_ptr<ProcessMetrics> metrics =
      ProcessMetrics::CreateCurrentProcessMetrics();

  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  int fd_count = metrics->GetOpenFdCount();
  EXPECT_GT(fd_count, 0);
  File file(temp_dir.GetPath().AppendASCII("file"),
            File::FLAG_CREATE | File::FLAG_WRITE);
  int new_fd_count = metrics->GetOpenFdCount();
  EXPECT_GT(new_fd_count, 0);
  EXPECT_EQ(new_fd_count, fd_count + 1);
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

TEST(ProcessMetricsTestLinux, GetPageFaultCounts) {
  std::unique_ptr<ProcessMetrics> process_metrics =
      ProcessMetrics::CreateCurrentProcessMetrics();

  PageFaultCounts counts;
  ASSERT_TRUE(process_metrics->GetPageFaultCounts(&counts));
  ASSERT_GT(counts.minor, 0);
  ASSERT_GE(counts.major, 0);

  // Allocate and touch memory. Touching it is required to make sure that the
  // page fault count goes up, as memory is typically mapped lazily.
  {
    const size_t kMappedSize = 4 << 20;  // 4 MiB.

    WritableSharedMemoryRegion region =
        WritableSharedMemoryRegion::Create(kMappedSize);
    ASSERT_TRUE(region.IsValid());

    WritableSharedMemoryMapping mapping = region.Map();
    ASSERT_TRUE(mapping.IsValid());

    memset(mapping.memory(), 42, kMappedSize);
  }

  PageFaultCounts counts_after;
  ASSERT_TRUE(process_metrics->GetPageFaultCounts(&counts_after));
  ASSERT_GT(counts_after.minor, counts.minor);
  ASSERT_GE(counts_after.major, counts.major);
}

TEST(ProcessMetricsTestLinux, GetCumulativeCPUUsagePerThread) {
  std::unique_ptr<ProcessMetrics> metrics =
      ProcessMetrics::CreateCurrentProcessMetrics();

  Thread thread1("thread1");
  thread1.StartAndWaitForTesting();
  ASSERT_TRUE(thread1.IsRunning());

  std::vector<std::string> vec1;
  thread1.task_runner()->PostTask(FROM_HERE, BindOnce(&BusyWork, &vec1));

  ProcessMetrics::CPUUsagePerThread prev_thread_times;
  EXPECT_TRUE(metrics->GetCumulativeCPUUsagePerThread(prev_thread_times));

  // Should have at least the test runner thread and the thread spawned above.
  EXPECT_GE(prev_thread_times.size(), 2u);
  EXPECT_TRUE(ranges::any_of(
      prev_thread_times,
      [&thread1](const std::pair<PlatformThreadId, base::TimeDelta>& entry) {
        return entry.first == thread1.GetThreadId();
      }));
  EXPECT_TRUE(ranges::any_of(
      prev_thread_times,
      [](const std::pair<PlatformThreadId, base::TimeDelta>& entry) {
        return entry.first == base::PlatformThread::CurrentId();
      }));

  for (const auto& entry : prev_thread_times) {
    EXPECT_GE(entry.second, base::TimeDelta());
  }

  thread1.Stop();

  ProcessMetrics::CPUUsagePerThread current_thread_times;
  EXPECT_TRUE(metrics->GetCumulativeCPUUsagePerThread(current_thread_times));

  // The stopped thread may still be reported until the kernel cleans it up.
  EXPECT_GE(prev_thread_times.size(), 1u);
  EXPECT_TRUE(ranges::any_of(
      current_thread_times,
      [](const std::pair<PlatformThreadId, base::TimeDelta>& entry) {
        return entry.first == base::PlatformThread::CurrentId();
      }));

  // Reported times should not decrease.
  for (const auto& entry : current_thread_times) {
    auto prev_it = ranges::find_if(
        prev_thread_times,
        [&entry](
            const std::pair<PlatformThreadId, base::TimeDelta>& prev_entry) {
          return entry.first == prev_entry.first;
        });

    if (prev_it != prev_thread_times.end()) {
      EXPECT_GE(entry.second, prev_it->second);
    }
  }
}
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) ||
        // BUILDFLAG(IS_CHROMEOS)

}  // namespace base::debug
