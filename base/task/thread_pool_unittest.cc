// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool.h"

#include "base/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ThreadPool, PostTaskAndReplyWithResultThreeArgs) {
  base::test::TaskEnvironment env;

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce([]() { return 3; }),
      base::OnceCallback<void(int)>(
          base::BindLambdaForTesting([&run_loop](int x) {
            EXPECT_EQ(x, 3);
            run_loop.Quit();
          })));
  run_loop.Run();
}

TEST(ThreadPool, PostTaskAndReplyWithResultFourArgs) {
  base::test::TaskEnvironment env;

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, /*traits=*/{}, base::BindOnce([]() { return 3; }),
      base::OnceCallback<void(int)>(
          base::BindLambdaForTesting([&run_loop](int x) {
            EXPECT_EQ(x, 3);
            run_loop.Quit();
          })));
  run_loop.Run();
}

}  // namespace base
