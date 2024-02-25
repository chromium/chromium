// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/concurrent_callbacks.h"

#include <vector>

#include "base/functional/callback.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

TEST(ConcurrentCallbacksTest, Empty) {
  test::SingleThreadTaskEnvironment task_environment;

  ConcurrentCallbacks<size_t> concurrent;

  test::TestFuture<std::vector<size_t>> future;
  std::move(concurrent).Done(future.GetCallback());

  std::vector<size_t> values = future.Take();
  EXPECT_TRUE(values.empty());
}

TEST(ConcurrentCallbacksTest, RunBeforeDone) {
  test::SingleThreadTaskEnvironment task_environment;

  ConcurrentCallbacks<size_t> concurrent;

  for (size_t i = 0; i < 10; ++i) {
    concurrent.CreateCallback().Run(i);
  }

  test::TestFuture<std::vector<size_t>> future;
  std::move(concurrent).Done(future.GetCallback());

  EXPECT_FALSE(future.IsReady());

  std::vector<size_t> values = future.Take();
  EXPECT_EQ(values.size(), 10u);
  for (size_t i = 0; i < values.size(); ++i) {
    EXPECT_EQ(values[i], i);
  }
}

TEST(ConcurrentCallbacksTest, RunAfterDone) {
  test::SingleThreadTaskEnvironment task_environment;

  ConcurrentCallbacks<size_t> concurrent;

  std::vector<base::OnceCallback<void(size_t)>> callbacks;
  for (size_t i = 0; i < 10; ++i) {
    callbacks.push_back(concurrent.CreateCallback());
  }

  test::TestFuture<std::vector<size_t>> future;
  std::move(concurrent).Done(future.GetCallback());

  for (size_t i = 0; i < callbacks.size(); ++i) {
    std::move(callbacks[i]).Run(i);
  }

  EXPECT_FALSE(future.IsReady());

  std::vector<size_t> values = future.Take();
  EXPECT_EQ(values.size(), 10u);
  for (size_t i = 0; i < values.size(); ++i) {
    EXPECT_EQ(values[i], i);
  }
}

TEST(ConcurrentCallbacksTest, CallbacksOutliveObject) {
  test::SingleThreadTaskEnvironment task_environment;

  std::vector<base::OnceCallback<void(size_t)>> callbacks;
  test::TestFuture<std::vector<size_t>> future;

  {
    ConcurrentCallbacks<size_t> concurrent;
    for (size_t i = 0; i < 10; ++i) {
      callbacks.push_back(concurrent.CreateCallback());
    }
    std::move(concurrent).Done(future.GetCallback());
  }

  for (size_t i = 0; i < callbacks.size(); ++i) {
    std::move(callbacks[i]).Run(i);
  }

  EXPECT_FALSE(future.IsReady());

  std::vector<size_t> values = future.Take();
  EXPECT_EQ(values.size(), 10u);
  for (size_t i = 0; i < values.size(); ++i) {
    EXPECT_EQ(values[i], i);
  }
}

TEST(ConcurrentCallbacksTest, CallbackAcceptsConstRef) {
  test::SingleThreadTaskEnvironment task_environment;

  ConcurrentCallbacks<const size_t&> concurrent;

  std::vector<base::OnceCallback<void(const size_t&)>> callbacks;
  for (size_t i = 0; i < 10; ++i) {
    callbacks.push_back(concurrent.CreateCallback());
  }

  test::TestFuture<std::vector<size_t>> future;
  std::move(concurrent).Done(future.GetCallback());

  for (size_t i = 0; i < callbacks.size(); ++i) {
    std::move(callbacks[i]).Run(i);
  }

  EXPECT_FALSE(future.IsReady());

  std::vector<size_t> values = future.Take();
  EXPECT_EQ(values.size(), 10u);
  for (size_t i = 0; i < values.size(); ++i) {
    EXPECT_EQ(values[i], i);
  }
}

}  // namespace

}  // namespace base
