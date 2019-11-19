// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <stddef.h>
#include <stdint.h>

#include <sstream>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/shared_memory_mapping.h"
#include "base/memory/writable_shared_memory_region.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/system/sys_info.h"
#include "base/test/multiprocess_test.h"
#include "base/threading/thread.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/multiprocess_func_list.h"

#if defined(OS_MACOSX)
#include <sys/mman.h>
#endif

namespace base {
namespace debug {

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_WIN)
namespace {

void BusyWork(std::vector<std::string>* vec) {
  int64_t test_value = 0;
  for (int i = 0; i < 100000; ++i) {
    ++test_value;
    vec->push_back(NumberToString(test_value));
  }
}

}  // namespace
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

// Tests for SystemMetrics.
// Exists as a class so it can be a friend of SystemMetrics.
class SystemMetricsTest : public testing::Test {
 public:
  SystemMetricsTest() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(SystemMetricsTest);
};

#if defined(OS_LINUX) || defined(OS_ANDROID)
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
#if defined(OS_CHROMEOS)
  EXPECT_EQ(meminfo.shmem, 140204);
  EXPECT_EQ(meminfo.slab, 54212);
#endif
  EXPECT_EQ(355725,
            base::SysInfo::AmountOfAvailablePhysicalMemory(meminfo) / 1024);
  // Simulate as if there is no MemAvailable.
  meminfo.available = 0;
  EXPECT_EQ(374448,
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
  EXPECT_EQ(69936,
            base::SysInfo::AmountOfAvailablePhysicalMemory(meminfo) / 1024);
}

TEST_F(SystemMetricsTest, ParseVmstat) {
  VmStatInfo vmstat;
  // part of vmstat from a 3.2 kernel with numa enabled
  const char valid_input1[] =
      "nr_free_pages 905104\n"
      "nr_inactive_anon 142478"
      "nr_active_anon 1520046\n"
      "nr_inactive_file 4481001\n"
      "nr_active_file 8313439\n"
      "nr_unevictable 5044\n"
      "nr_mlock 5044\n"
      "nr_anon_pages 1633780\n"
      "nr_mapped 104742\n"
      "nr_file_pages 12828218\n"
      "nr_dirty 245\n"
      "nr_writeback 0\n"
      "nr_slab_reclaimable 831609\n"
      "nr_slab_unreclaimable 41164\n"
      "nr_page_table_pages 31470\n"
      "nr_kernel_stack 1735\n"
      "nr_unstable 0\n"
      "nr_bounce 0\n"
      "nr_vmscan_write 406\n"
      "nr_vmscan_immediate_reclaim 281\n"
      "nr_writeback_temp 0\n"
      "nr_isolated_anon 0\n"
      "nr_isolated_file 0\n"
      "nr_shmem 28820\n"
      "nr_dirtied 84674644\n"
      "nr_written 75307109\n"
      "nr_anon_transparent_hugepages 0\n"
      "nr_dirty_threshold 1536206\n"
      "nr_dirty_background_threshold 768103\n"
      "pgpgin 30777108\n"
      "pgpgout 319023278\n"
      "pswpin 179\n"
      "pswpout 406\n"
      "pgalloc_dma 0\n"
      "pgalloc_dma32 20833399\n"
      "pgalloc_normal 1622609290\n"
      "pgalloc_movable 0\n"
      "pgfree 1644355583\n"
      "pgactivate 75391882\n"
      "pgdeactivate 4121019\n"
      "pgfault 2542879679\n"
      "pgmajfault 487192\n";
  const char valid_input2[] =
      "nr_free_pages 180125\n"
      "nr_inactive_anon 51\n"
      "nr_active_anon 38832\n"
      "nr_inactive_file 50171\n"
      "nr_active_file 47510\n"
      "nr_unevictable 0\n"
      "nr_mlock 0\n"
      "nr_anon_pages 38825\n"
      "nr_mapped 24043\n"
      "nr_file_pages 97733\n"
      "nr_dirty 0\n"
      "nr_writeback 0\n"
      "nr_slab_reclaimable 4032\n"
      "nr_slab_unreclaimable 2848\n"
      "nr_page_table_pages 1505\n"
      "nr_kernel_stack 626\n"
      "nr_unstable 0\n"
      "nr_bounce 0\n"
      "nr_vmscan_write 0\n"
      "nr_vmscan_immediate_reclaim 0\n"
      "nr_writeback_temp 0\n"
      "nr_isolated_anon 0\n"
      "nr_isolated_file 0\n"
      "nr_shmem 58\n"
      "nr_dirtied 435358\n"
      "nr_written 401258\n"
      "nr_anon_transparent_hugepages 0\n"
      "nr_dirty_threshold 18566\n"
      "nr_dirty_background_threshold 4641\n"
      "pgpgin 299464\n"
      "pgpgout 2437788\n"
      "pswpin 12\n"
      "pswpout 901\n"
      "pgalloc_normal 144213030\n"
      "pgalloc_high 164501274\n"
      "pgalloc_movable 0\n"
      "pgfree 308894908\n"
      "pgactivate 239320\n"
      "pgdeactivate 1\n"
      "pgfault 716044601\n"
      "pgmajfault 2023\n"
      "pgrefill_normal 0\n"
      "pgrefill_high 0\n"
      "pgrefill_movable 0\n";
  EXPECT_TRUE(ParseProcVmstat(valid_input1, &vmstat));
  EXPECT_EQ(179LU, vmstat.pswpin);
  EXPECT_EQ(406LU, vmstat.pswpout);
  EXPECT_EQ(487192LU, vmstat.pgmajfault);
  EXPECT_TRUE(ParseProcVmstat(valid_input2, &vmstat));
  EXPECT_EQ(12LU, vmstat.pswpin);
  EXPECT_EQ(901LU, vmstat.pswpout);
  EXPECT_EQ(2023LU, vmstat.pgmajfault);

  const char missing_pgmajfault_input[] =
      "pswpin 12\n"
      "pswpout 901\n";
  EXPECT_FALSE(ParseProcVmstat(missing_pgmajfault_input, &vmstat));
  const char empty_input[] = "";
  EXPECT_FALSE(ParseProcVmstat(empty_input, &vmstat));
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_WIN)

// Test that ProcessMetrics::GetPlatformIndependentCPUUsage() doesn't return
// negative values when the number of threads running on the process decreases
// between two successive calls to it.
TEST_F(SystemMetricsTest, TestNoNegativeCpuUsage) {
  ProcessHandle handle = GetCurrentProcessHandle();
  std::unique_ptr<ProcessMetrics> metrics(
      ProcessMetrics::CreateProcessMetrics(handle));

  EXPECT_GE(metrics->GetPlatformIndependentCPUUsage(), 0.0);
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

  TimeDelta prev_cpu_usage = metrics->GetCumulativeCPUUsage();
  EXPECT_GE(prev_cpu_usage, TimeDelta());
  EXPECT_GE(metrics->GetPlatformIndependentCPUUsage(), 0.0);

  thread1.Stop();
  TimeDelta current_cpu_usage = metrics->GetCumulativeCPUUsage();
  EXPECT_GE(current_cpu_usage, prev_cpu_usage);
  prev_cpu_usage = current_cpu_usage;
  EXPECT_GE(metrics->GetPlatformIndependentCPUUsage(), 0.0);

  thread2.Stop();
  current_cpu_usage = metrics->GetCumulativeCPUUsage();
  EXPECT_GE(current_cpu_usage, prev_cpu_usage);
  prev_cpu_usage = current_cpu_usage;
  EXPECT_GE(metrics->GetPlatformIndependentCPUUsage(), 0.0);

  thread3.Stop();
  current_cpu_usage = metrics->GetCumulativeCPUUsage();
  EXPECT_GE(current_cpu_usage, prev_cpu_usage);
  EXPECT_GE(metrics->GetPlatformIndependentCPUUsage(), 0.0);
}

#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
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
#endif  // defined(OS_CHROMEOS)

#if defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) || \
    defined(OS_ANDROID)
TEST(SystemMetrics2Test, GetSystemMemoryInfo) {
  SystemMemoryInfoKB info;
  EXPECT_TRUE(GetSystemMemoryInfo(&info));

  // Ensure each field received a value.
  EXPECT_GT(info.total, 0);
#if defined(OS_WIN)
  EXPECT_GT(info.avail_phys, 0);
#else
  EXPECT_GT(info.free, 0);
#endif
#if defined(OS_LINUX) || defined(OS_ANDROID)
  EXPECT_GT(info.buffers, 0);
  EXPECT_GT(info.cached, 0);
  EXPECT_GT(info.active_anon, 0);
  EXPECT_GT(info.inactive_anon, 0);
  EXPECT_GT(info.active_file, 0);
  EXPECT_GT(info.inactive_file, 0);
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

  // All the values should be less than the total amount of memory.
#if !defined(OS_WIN) && !defined(OS_IOS)
  // TODO(crbug.com/711450): re-enable the following assertion on iOS.
  EXPECT_LT(info.free, info.total);
#endif
#if defined(OS_LINUX) || defined(OS_ANDROID)
  EXPECT_LT(info.buffers, info.total);
  EXPECT_LT(info.cached, info.total);
  EXPECT_LT(info.active_anon, info.total);
  EXPECT_LT(info.inactive_anon, info.total);
  EXPECT_LT(info.active_file, info.total);
  EXPECT_LT(info.inactive_file, info.total);
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

#if defined(OS_MACOSX) || defined(OS_IOS)
  EXPECT_GT(info.file_backed, 0);
#endif

#if defined(OS_CHROMEOS)
  // Chrome OS exposes shmem.
  EXPECT_GT(info.shmem, 0);
  EXPECT_LT(info.shmem, info.total);
  // Chrome unit tests are not run on actual Chrome OS hardware, so gem_objects
  // and gem_size cannot be tested here.
#endif
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX) || defined(OS_LINUX) ||
        // defined(OS_ANDROID)

#if defined(OS_LINUX) || defined(OS_ANDROID)
TEST(ProcessMetricsTest, ParseProcStatCPU) {
  // /proc/self/stat for a process running "top".
  const char kTopStat[] = "960 (top) S 16230 960 16230 34818 960 "
      "4202496 471 0 0 0 "
      "12 16 0 0 "  // <- These are the goods.
      "20 0 1 0 121946157 15077376 314 18446744073709551615 4194304 "
      "4246868 140733983044336 18446744073709551615 140244213071219 "
      "0 0 0 138047495 0 0 0 17 1 0 0 0 0 0";
  EXPECT_EQ(12 + 16, ParseProcStatCPU(kTopStat));

  // cat /proc/self/stat on a random other machine I have.
  const char kSelfStat[] = "5364 (cat) R 5354 5364 5354 34819 5364 "
      "0 142 0 0 0 "
      "0 0 0 0 "  // <- No CPU, apparently.
      "16 0 1 0 1676099790 2957312 114 4294967295 134512640 134528148 "
      "3221224832 3221224344 3086339742 0 0 0 0 0 0 0 17 0 0 0";

  EXPECT_EQ(0, ParseProcStatCPU(kSelfStat));

  // Some weird long-running process with a weird name that I created for the
  // purposes of this test.
  const char kWeirdNameStat[] = "26115 (Hello) You ()))  ) R 24614 26115 24614"
      " 34839 26115 4218880 227 0 0 0 "
      "5186 11 0 0 "
      "20 0 1 0 36933953 4296704 90 18446744073709551615 4194304 4196116 "
      "140735857761568 140735857761160 4195644 0 0 0 0 0 0 0 17 14 0 0 0 0 0 "
      "6295056 6295616 16519168 140735857770710 140735857770737 "
      "140735857770737 140735857774557 0";
  EXPECT_EQ(5186 + 11, ParseProcStatCPU(kWeirdNameStat));
}
#endif  // defined(OS_LINUX) || defined(OS_ANDROID)

// Disable on Android because base_unittests runs inside a Dalvik VM that
// starts and stop threads (crbug.com/175563).
#if defined(OS_LINUX)
// http://crbug.com/396455
TEST(ProcessMetricsTest, DISABLED_GetNumberOfThreads) {
  const ProcessHandle current = GetCurrentProcessHandle();
  const int initial_threads = GetNumberOfThreads(current);
  ASSERT_GT(initial_threads, 0);
  const int kNumAdditionalThreads = 10;
  {
    std::unique_ptr<Thread> my_threads[kNumAdditionalThreads];
    for (int i = 0; i < kNumAdditionalThreads; ++i) {
      my_threads[i].reset(new Thread("GetNumberOfThreadsTest"));
      my_threads[i]->Start();
      ASSERT_EQ(GetNumberOfThreads(current), initial_threads + 1 + i);
    }
  }
  // The Thread destructor will stop them.
  ASSERT_EQ(initial_threads, GetNumberOfThreads(current));
}
#endif  // defined(OS_LINUX)

#if defined(OS_LINUX) || (defined(OS_MACOSX) && !defined(OS_IOS))
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
  while (!CheckEvent(signal_dir, signal_file))
    PlatformThread::Sleep(TimeDelta::FromMilliseconds(10));
}

// Subprocess to test the number of open file descriptors.
MULTIPROCESS_TEST_MAIN(ChildMain) {
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
  while (true)
    PlatformThread::Sleep(TimeDelta::FromSeconds(1));
  return 0;
}

}  // namespace

TEST(ProcessMetricsTest, GetChildOpenFdCount) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath temp_path = temp_dir.GetPath();
  CommandLine child_command_line(GetMultiProcessTestChildBaseCommandLine());
  child_command_line.AppendSwitchPath(kTempDirFlag, temp_path);
  Process child = SpawnMultiProcessTestChild(
      ChildMainString, child_command_line, LaunchOptions());
  ASSERT_TRUE(child.IsValid());

  WaitForEvent(temp_path, kSignalReady);

  std::unique_ptr<ProcessMetrics> metrics =
#if defined(OS_MACOSX)
      ProcessMetrics::CreateProcessMetrics(child.Handle(), nullptr);
#else
      ProcessMetrics::CreateProcessMetrics(child.Handle());
#endif  // defined(OS_MACOSX)

  const int fd_count = metrics->GetOpenFdCount();
  EXPECT_GE(fd_count, 0);

  ASSERT_TRUE(SignalEvent(temp_path, kSignalReadyAck));
  WaitForEvent(temp_path, kSignalOpened);

  EXPECT_EQ(fd_count + kChildNumFilesToOpen, metrics->GetOpenFdCount());
  ASSERT_TRUE(SignalEvent(temp_path, kSignalOpenedAck));

  WaitForEvent(temp_path, kSignalClosed);

  EXPECT_EQ(fd_count, metrics->GetOpenFdCount());

  ASSERT_TRUE(child.Terminate(0, true));
}

TEST(ProcessMetricsTest, GetOpenFdCount) {
  base::ProcessHandle process = base::GetCurrentProcessHandle();
  std::unique_ptr<base::ProcessMetrics> metrics =
#if defined(OS_MACOSX)
      ProcessMetrics::CreateProcessMetrics(process, nullptr);
#else
      ProcessMetrics::CreateProcessMetrics(process);
#endif  // defined(OS_MACOSX)

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

#endif  // defined(OS_LINUX) || (defined(OS_MACOSX) && !defined(OS_IOS))

#if defined(OS_ANDROID) || defined(OS_LINUX)
TEST(ProcessMetricsTestLinux, GetPageFaultCounts) {
  std::unique_ptr<base::ProcessMetrics> process_metrics(
      base::ProcessMetrics::CreateProcessMetrics(
          base::GetCurrentProcessHandle()));

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
#endif  // defined(OS_ANDROID) || defined(OS_LINUX)

#if defined(OS_WIN)
TEST(ProcessMetricsTest, GetDiskUsageBytesPerSecond) {
  ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const FilePath temp_path = temp_dir.GetPath().AppendASCII("dummy");

  ProcessHandle handle = GetCurrentProcessHandle();
  std::unique_ptr<ProcessMetrics> metrics(
      ProcessMetrics::CreateProcessMetrics(handle));

  // First access is returning zero bytes.
  EXPECT_EQ(metrics->GetDiskUsageBytesPerSecond(), 0U);

  // Write a megabyte on disk.
  const int kMegabyte = 1024 * 1014;
  std::string data(kMegabyte, 'x');
  ASSERT_EQ(kMegabyte, base::WriteFile(temp_path, data.c_str(), data.size()));

  // Validate that the counters move up.
  EXPECT_GT(metrics->GetDiskUsageBytesPerSecond(), 0U);
}
#endif  // defined(OS_WIN)

}  // namespace debug
}  // namespace base
