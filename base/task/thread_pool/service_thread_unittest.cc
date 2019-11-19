// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/service_thread.h"

#include <string>

#include "base/bind.h"
#include "base/debug/stack_trace.h"
#include "base/task/thread_pool/thread_pool_impl.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {

// Verifies that |query| is found on the current stack. Ignores failures if this
// configuration doesn't have symbols.
void VerifyHasStringOnStack(const std::string& query) {
  const std::string stack = debug::StackTrace().ToString();
  SCOPED_TRACE(stack);
  const bool found_on_stack = stack.find(query) != std::string::npos;
  const bool stack_has_symbols =
      stack.find("WorkerThread") != std::string::npos;
  EXPECT_TRUE(found_on_stack || !stack_has_symbols) << query;
}

}  // namespace

#if defined(OS_POSIX)
// Many POSIX bots flakily crash on |debug::StackTrace().ToString()|,
// https://crbug.com/840429.
#define MAYBE_StackHasIdentifyingFrame DISABLED_StackHasIdentifyingFrame
#else
#define MAYBE_StackHasIdentifyingFrame StackHasIdentifyingFrame
#endif

TEST(ThreadPoolServiceThreadTest, MAYBE_StackHasIdentifyingFrame) {
  ServiceThread service_thread(nullptr, DoNothing());
  service_thread.Start();

  service_thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&VerifyHasStringOnStack, "ServiceThread"));

  service_thread.FlushForTesting();
}

// Integration test verifying that a service thread running in a fully
// integrated ThreadPool environment results in reporting
// HeartbeatLatencyMicroseconds metrics.
TEST(ThreadPoolServiceThreadIntegrationTest, HeartbeatLatencyReport) {
  ServiceThread::SetHeartbeatIntervalForTesting(TimeDelta::FromMilliseconds(1));

  ThreadPoolInstance::Set(std::make_unique<internal::ThreadPoolImpl>("Test"));
  ThreadPoolInstance::Get()->StartWithDefaultParams();

  static constexpr const char* kExpectedMetrics[] = {
      "ThreadPool.HeartbeatLatencyMicroseconds.Test."
      "UserBlockingTaskPriority",
      "ThreadPool.HeartbeatLatencyMicroseconds.Test."
      "UserVisibleTaskPriority",
      "ThreadPool.HeartbeatLatencyMicroseconds.Test."
      "BackgroundTaskPriority"};

  // Each report hits a single histogram above (randomly selected). But 1000
  // reports should touch all histograms at least once the vast majority of the
  // time.
  constexpr TimeDelta kReasonableTimeout = TimeDelta::FromSeconds(1);
  constexpr TimeDelta kBusyWaitTime = TimeDelta::FromMilliseconds(100);

  const TimeTicks start_time = TimeTicks::Now();

  HistogramTester tester;
  for (const char* expected_metric : kExpectedMetrics) {
    while (tester.GetAllSamples(expected_metric).empty()) {
      if (TimeTicks::Now() - start_time > kReasonableTimeout)
        LOG(WARNING) << "Waiting a while for " << expected_metric;
      PlatformThread::Sleep(kBusyWaitTime);
    }
  }

  ThreadPoolInstance::Get()->JoinForTesting();
  ThreadPoolInstance::Set(nullptr);

  ServiceThread::SetHeartbeatIntervalForTesting(TimeDelta());
}

}  // namespace internal
}  // namespace base
