// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/perf/process_type_collector.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/test/metrics/histogram_tester.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace metrics {

namespace {

void GetExampleProcessTypeDataset(std::string* ps_output,
                                  std::map<uint32_t, Process>* process_types) {
  *ps_output = R"text(PID   CMD
    1000 /opt/google/chrome/chrome --type=
    1500 /opt/google/chrome/chrome --type= --some-flag
    2000 /opt/google/chrome/chrome --type=renderer --enable-logging
    3000 /opt/google/chrome/chrome --type=gpu-process
    4000 /opt/google/chrome/chrome --log-level=1 --type=utility
    5000 /opt/google/chrome/chrome --type=zygote
    6000 /opt/google/chrome/chrome --type=ppapi
    7000 /opt/google/chrome/chrome --type=ppapi-broker
    7100 /opt/google/chrome/chrome --type=random-type
    7200 /opt/google/chrome/chrome --no_type
  129000 /opt/google/chrome/chrome --ppapi-flash-path=..../libpepflashplayer.so
  180000 [kswapd0])text";
  process_types->insert(google::protobuf::MapPair<uint32_t, Process>(
      1000, Process::BROWSER_PROCESS));
  process_types->insert(google::protobuf::MapPair<uint32_t, Process>(
      1500, Process::BROWSER_PROCESS));
  process_types->insert(google::protobuf::MapPair<uint32_t, Process>(
      2000, Process::RENDERER_PROCESS));
  process_types->insert(
      google::protobuf::MapPair<uint32_t, Process>(3000, Process::GPU_PROCESS));
  process_types->insert(google::protobuf::MapPair<uint32_t, Process>(
      4000, Process::UTILITY_PROCESS));
  process_types->insert(google::protobuf::MapPair<uint32_t, Process>(
      5000, Process::ZYGOTE_PROCESS));
  process_types->insert(google::protobuf::MapPair<uint32_t, Process>(
      6000, Process::PPAPI_PLUGIN_PROCESS));
  process_types->insert(google::protobuf::MapPair<uint32_t, Process>(
      7000, Process::PPAPI_BROKER_PROCESS));
  process_types->insert(google::protobuf::MapPair<uint32_t, Process>(
      7100, Process::OTHER_PROCESS));
  process_types->insert(google::protobuf::MapPair<uint32_t, Process>(
      7200, Process::BROWSER_PROCESS));
  process_types->insert(google::protobuf::MapPair<uint32_t, Process>(
      129000, Process::BROWSER_PROCESS));
}

void GetExampleThreadTypeDataset(std::string* ps_output,
                                 std::map<uint32_t, Thread>* thread_types) {
  *ps_output = R"text(  PID   LWP   COMMAND     CMD
      1     1 init              /sbin/init
  12000 12000 chrome            /opt/google/chrome/chrome --ppapi-flash-path=...
   3000  1107 irq/5-E:<L0000>   [irq/5-E:<L0000>]
   4000  4726 Chrome_IOThread   /opt/google/chrome/chrome --ppapi-flash-path=...
  10000 12107 Chrome_ChildIOT   /opt/google/chrome/chrome --type=gpu-process
  11000 12207 VizCompositorTh   /opt/google/chrome/chrome --type=gpu-process
  12103 12112 Compositor/7      /opt/google/chrome/chrome --type=gpu-process
  12304 12699 Compositor        /opt/google/chrome/chrome --type=gpu-process
  13001 13521 GpuMemoryThread   /opt/google/chrome/chrome --type=renderer
  15000 15112 ThreadPoolForeg   /opt/google/chrome/chrome --type=renderer
  16000 16112 CompositorTileW   /opt/google/chrome/chrome --type=renderer
  12345 12456 OtherThread       /opt/google/chrome/chrome --ppapi-flash-path=...
  13456 13566 Compositor/6      non_chrome_exec --some-flag=foo)text";
  thread_types->insert(
      google::protobuf::MapPair<uint32_t, Thread>(12000, Thread::MAIN_THREAD));
  thread_types->insert(
      google::protobuf::MapPair<uint32_t, Thread>(4726, Thread::IO_THREAD));
  thread_types->insert(
      google::protobuf::MapPair<uint32_t, Thread>(12107, Thread::IO_THREAD));
  thread_types->insert(google::protobuf::MapPair<uint32_t, Thread>(
      12207, Thread::COMPOSITOR_THREAD));
  thread_types->insert(google::protobuf::MapPair<uint32_t, Thread>(
      12112, Thread::COMPOSITOR_THREAD));
  thread_types->insert(google::protobuf::MapPair<uint32_t, Thread>(
      12699, Thread::COMPOSITOR_THREAD));
  thread_types->insert(google::protobuf::MapPair<uint32_t, Thread>(
      15112, Thread::THREAD_POOL_THREAD));
  thread_types->insert(google::protobuf::MapPair<uint32_t, Thread>(
      13521, Thread::GPU_MEMORY_THREAD));
  thread_types->insert(google::protobuf::MapPair<uint32_t, Thread>(
      16112, Thread::COMPOSITOR_TILE_WORKER_THREAD));
  thread_types->insert(
      google::protobuf::MapPair<uint32_t, Thread>(12456, Thread::OTHER_THREAD));
}

class TestProcessTypeCollector : public ProcessTypeCollector {
 public:
  using ProcessTypeCollector::CollectionAttemptStatus;
  using ProcessTypeCollector::ParseProcessTypes;
  using ProcessTypeCollector::ParseThreadTypes;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestProcessTypeCollector);
};

}  // namespace

TEST(ProcessTypeCollectorTest, ValidProcessTypeInput) {
  std::map<uint32_t, Process> want_process_types;
  std::string input;
  GetExampleProcessTypeDataset(&input, &want_process_types);
  EXPECT_FALSE(input.empty());
  EXPECT_FALSE(want_process_types.empty());

  base::HistogramTester histogram_tester;
  std::map<uint32_t, Process> got_process_types =
      TestProcessTypeCollector::ParseProcessTypes(input);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.CWP.CollectProcessTypes",
      TestProcessTypeCollector::CollectionAttemptStatus::kProcessTypeSuccess,
      1);
  EXPECT_EQ(got_process_types, want_process_types);
}

TEST(ProcessTypeCollectorTest, ProcessTypeInputWithCorruptedLine) {
  std::string input = R"text(PPID   CMD   COMMAND
  PID /opt/google/chrome/chrome --type=
  1000 /opt/google/chrome/chrome --type=)text";
  std::map<uint32_t, Process> want_process_types;
  want_process_types.emplace(1000, Process::BROWSER_PROCESS);

  base::HistogramTester histogram_tester;
  std::map<uint32_t, Process> got_process_types =
      TestProcessTypeCollector::ParseProcessTypes(input);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.CWP.CollectProcessTypes",
      TestProcessTypeCollector::CollectionAttemptStatus::kProcessTypeTruncated,
      1);
  EXPECT_EQ(got_process_types, want_process_types);
}

TEST(ProcessTypeCollectorTest, ProcessTypeInputWithDuplicatePIDs) {
  std::string input = R"text(PID   CMD
  1000 /opt/google/chrome/chrome --type=
  1000 /opt/google/chrome/chrome --type=)text";
  std::map<uint32_t, Process> want_process_types;
  want_process_types.emplace(1000, Process::BROWSER_PROCESS);

  base::HistogramTester histogram_tester;
  std::map<uint32_t, Process> got_process_types =
      TestProcessTypeCollector::ParseProcessTypes(input);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.CWP.CollectProcessTypes",
      TestProcessTypeCollector::CollectionAttemptStatus::kProcessTypeTruncated,
      1);
  EXPECT_EQ(got_process_types, want_process_types);
}

TEST(ProcessTypeCollectorTest, ProcessTypeInputWithEmptyLine) {
  std::string input = R"text(PPID   CMD   COMMAND
  1000 /opt/google/chrome/chrome --type=
  )text";
  std::map<uint32_t, Process> want_process_types;
  want_process_types.emplace(1000, Process::BROWSER_PROCESS);

  base::HistogramTester histogram_tester;
  std::map<uint32_t, Process> got_process_types =
      TestProcessTypeCollector::ParseProcessTypes(input);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.CWP.CollectProcessTypes",
      TestProcessTypeCollector::CollectionAttemptStatus::kProcessTypeTruncated,
      1);
  EXPECT_EQ(got_process_types, want_process_types);
}

TEST(ProcessTypeCollectorTest, ValidThreadTypeInput) {
  std::map<uint32_t, Thread> want_thread_types;
  std::string input;
  GetExampleThreadTypeDataset(&input, &want_thread_types);
  EXPECT_FALSE(input.empty());
  EXPECT_FALSE(want_thread_types.empty());

  base::HistogramTester histogram_tester;
  std::map<uint32_t, Thread> got_thread_types =
      TestProcessTypeCollector::ParseThreadTypes(input);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.CWP.CollectProcessTypes",
      TestProcessTypeCollector::CollectionAttemptStatus::kThreadTypeSuccess, 1);
  EXPECT_EQ(got_thread_types, want_thread_types);
}

TEST(ProcessTypeCollectorTest, ThreadTypeInputWithCorruptedLine) {
  std::string input = R"text(  PID   LWPCMD
  PID     LWP init
  4000  4726 Chrome_IOThread /opt/google/chrome/chrome --ppapi-flash=...)text";
  std::map<uint32_t, Thread> want_thread_types;
  want_thread_types.emplace(4726, Thread::IO_THREAD);

  base::HistogramTester histogram_tester;
  std::map<uint32_t, Thread> got_thread_types =
      TestProcessTypeCollector::ParseThreadTypes(input);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.CWP.CollectProcessTypes",
      TestProcessTypeCollector::CollectionAttemptStatus::kThreadTypeTruncated,
      1);
  EXPECT_EQ(got_thread_types, want_thread_types);
}

TEST(ProcessTypeCollectorTest, ThreadTypeInputWithDuplicateTIDs) {
  std::string input = R"text(  PID   LWP CMD
  4000 4726 Chrome_IOThread  /opt/google/chrome/chrome --ppapi-flash-path=...
  4000 4726 VizCompositorTh  /opt/google/chrome/chrome --type=gpu-process)text";
  std::map<uint32_t, Thread> want_thread_types;
  want_thread_types.emplace(4726, Thread::IO_THREAD);

  base::HistogramTester histogram_tester;
  std::map<uint32_t, Thread> got_thread_types =
      TestProcessTypeCollector::ParseThreadTypes(input);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.CWP.CollectProcessTypes",
      TestProcessTypeCollector::CollectionAttemptStatus::kThreadTypeTruncated,
      1);
  EXPECT_EQ(got_thread_types, want_thread_types);
}

TEST(ProcessTypeCollectorTest, ThreadTypeInputWithEmptyLine) {
  std::string input = R"text(  PID   LWPCMD
  4000  4726 Chrome_IOThread /opt/google/chrome/chrome --ppapi-flash=...
  )text";
  std::map<uint32_t, Thread> want_thread_types;
  want_thread_types.emplace(4726, Thread::IO_THREAD);

  base::HistogramTester histogram_tester;
  std::map<uint32_t, Thread> got_thread_types =
      TestProcessTypeCollector::ParseThreadTypes(input);
  histogram_tester.ExpectBucketCount(
      "ChromeOS.CWP.CollectProcessTypes",
      TestProcessTypeCollector::CollectionAttemptStatus::kThreadTypeTruncated,
      1);
  EXPECT_EQ(got_thread_types, want_thread_types);
}

}  // namespace metrics
