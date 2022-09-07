// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/work_id_provider.h"

#include <limits>
#include <memory>
#include <utility>

#include "base/callback.h"
#include "base/test/bind.h"
#include "base/threading/simple_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

class TestThread : public SimpleThread {
 public:
  TestThread(OnceCallback<void(WorkIdProvider*)> validate_func)
      : SimpleThread("WorkIdProviderTestThread"),
        validate_func_(std::move(validate_func)) {}

  void Run() override {
    std::move(validate_func_).Run(WorkIdProvider::GetForCurrentThread());
  }

 private:
  OnceCallback<void(WorkIdProvider*)> validate_func_;
};

template <class Func>
void RunTest(const Func& func) {
  TestThread thread(BindLambdaForTesting(func));

  thread.Start();
  thread.Join();
}

}  // namespace

TEST(WorkIdProviderTest, StartsAtZero) {
  RunTest(
      [](WorkIdProvider* provider) { EXPECT_EQ(0u, provider->GetWorkId()); });
}

TEST(WorkIdProviderTest, Increment) {
  RunTest([](WorkIdProvider* provider) {
    provider->IncrementWorkIdForTesting();
    EXPECT_EQ(1u, provider->GetWorkId());

    provider->IncrementWorkIdForTesting();
    EXPECT_EQ(2u, provider->GetWorkId());

    provider->IncrementWorkIdForTesting();
    EXPECT_EQ(3u, provider->GetWorkId());
  });
}

TEST(WorkIdProviderTest, SkipsZeroOnOverflow) {
  RunTest([](WorkIdProvider* provider) {
    provider->SetCurrentWorkIdForTesting(
        std::numeric_limits<unsigned int>::max());
    provider->IncrementWorkIdForTesting();
    EXPECT_EQ(1u, provider->GetWorkId());
  });
}

}  // namespace base
