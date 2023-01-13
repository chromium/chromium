// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool/service_thread.h"

#include <string>

#include "base/debug/stack_trace.h"
#include "base/functional/bind.h"
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

#if BUILDFLAG(IS_POSIX)
// Many POSIX bots flakily crash on |debug::StackTrace().ToString()|,
// https://crbug.com/840429.
#define MAYBE_StackHasIdentifyingFrame DISABLED_StackHasIdentifyingFrame
#else
#define MAYBE_StackHasIdentifyingFrame StackHasIdentifyingFrame
#endif

TEST(ThreadPoolServiceThreadTest, MAYBE_StackHasIdentifyingFrame) {
  ServiceThread service_thread;
  service_thread.Start();

  service_thread.task_runner()->PostTask(
      FROM_HERE, BindOnce(&VerifyHasStringOnStack, "ServiceThread"));

  service_thread.FlushForTesting();
}

}  // namespace internal
}  // namespace base
