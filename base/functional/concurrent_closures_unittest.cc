// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/concurrent_closures.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

TEST(ConcurrentClosuresTest, Empty) {
  test::SingleThreadTaskEnvironment task_environment;

  ConcurrentClosures concurrent;

  test::TestFuture<void> future;
  std::move(concurrent).Done(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  EXPECT_TRUE(future.Wait());
}

TEST(ConcurrentClosuresTest, RunBeforeDone) {
  test::SingleThreadTaskEnvironment task_environment;

  ConcurrentClosures concurrent;

  for (size_t i = 0; i < 10; ++i) {
    concurrent.CreateClosure().Run();
  }

  test::TestFuture<void> future;
  std::move(concurrent).Done(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  EXPECT_TRUE(future.Wait());
}

TEST(ConcurrentClosuresTest, RunAfterDone) {
  test::SingleThreadTaskEnvironment task_environment;

  ConcurrentClosures concurrent;

  std::vector<base::OnceClosure> closures;
  for (size_t i = 0; i < 10; ++i) {
    closures.push_back(concurrent.CreateClosure());
  }

  test::TestFuture<void> future;
  std::move(concurrent).Done(future.GetCallback());

  for (base::OnceClosure& callback : closures) {
    std::move(callback).Run();
  }

  EXPECT_FALSE(future.IsReady());

  EXPECT_TRUE(future.Wait());
}

TEST(ConcurrentClosuresTest, CallbacksOutliveObject) {
  test::SingleThreadTaskEnvironment task_environment;

  std::vector<base::OnceClosure> closures;
  test::TestFuture<void> future;

  {
    ConcurrentClosures concurrent;
    for (size_t i = 0; i < 10; ++i) {
      closures.push_back(concurrent.CreateClosure());
    }
    std::move(concurrent).Done(future.GetCallback());
  }

  for (base::OnceClosure& callback : closures) {
    std::move(callback).Run();
  }

  EXPECT_FALSE(future.IsReady());

  EXPECT_TRUE(future.Wait());
}

}  // namespace

}  // namespace base
