// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/guest_os/infra/cached_callback.h"

#include <atomic>
#include <memory>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "chrome/browser/ash/borealis/testing/callback_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace guest_os {
namespace {

using ::testing::_;
using ::testing::Invoke;

enum class TestErrors {
  kFoo = 0,
};

class SuccessfulCache : public CachedCallback<std::string, TestErrors> {
 public:
  ~SuccessfulCache() override = default;

  // CachedCallback overrides.
  void Build(RealCallback callback) override {
    std::move(callback).Run(Success("success"));
  }
};

TEST(CachedCallbackTest, IsNullInitially) {
  SuccessfulCache sc;
  EXPECT_EQ(sc.MaybeGet(), nullptr);
  EXPECT_FALSE(sc.Invalidate());
}

TEST(CachedCallbackTest, SuccessIsPropagated) {
  SuccessfulCache sc;
  borealis::NiceCallbackFactory<void(SuccessfulCache::Result)> callbacks;
  EXPECT_CALL(callbacks, Call(_))
      .WillOnce(Invoke([](SuccessfulCache::Result result) {
        EXPECT_TRUE(result.has_value());
        EXPECT_EQ(*result.value(), "success");
      }));
  sc.Get(callbacks.BindOnce());
  EXPECT_NE(sc.MaybeGet(), nullptr);
}

class CountingCache : public SuccessfulCache {
 public:
  ~CountingCache() override = default;

  // CachedCallback overrides.
  void Build(RealCallback callback) override {
    build_cached_object_count++;
    return SuccessfulCache::Build(std::move(callback));
  }

  int build_cached_object_count = 0;
};

TEST(CachedCallbackTest, ReUsesSameObject) {
  CountingCache cc;
  EXPECT_EQ(cc.build_cached_object_count, 0);
  cc.Get(base::DoNothing());
  cc.Get(base::DoNothing());
  EXPECT_EQ(cc.build_cached_object_count, 1);
  cc.Invalidate();
  cc.Get(base::DoNothing());
  EXPECT_EQ(cc.build_cached_object_count, 2);
}

class FailureCache : public SuccessfulCache {
 public:
  ~FailureCache() override = default;

  // CachedCallback overrides.
  void Build(RealCallback callback) override {
    std::move(callback).Run(Failure(TestErrors::kFoo));
  }
};

TEST(CachedCallbackTest, FailureIsPropagated) {
  FailureCache fc;
  borealis::NiceCallbackFactory<void(FailureCache::Result)> callbacks;
  EXPECT_CALL(callbacks, Call(_))
      .WillOnce(Invoke([](FailureCache::Result result) {
        EXPECT_FALSE(result.has_value());
        EXPECT_EQ(result.error(), TestErrors::kFoo);
      }));
  fc.Get(callbacks.BindOnce());
}

class DelayedCache : public SuccessfulCache {
 public:
  ~DelayedCache() override = default;

  void DelayCallback(RealCallback callback) {
    // Busy loop until needed.
    while (delay) {
      continue;
    }
    std::move(callback).Run(
        SuccessfulCache::RealResult(std::make_unique<std::string>("success")));
  }

  // CachedCallback overrides.
  void Build(RealCallback callback) override {
    base::ThreadPool::PostTask(
        FROM_HERE, {base::MayBlock()},
        base::BindOnce(&DelayedCache::DelayCallback, base::Unretained(this),
                       std::move(callback)));
  }

  std::atomic_bool delay = true;
};

MATCHER(ExpectedTrue, "") {
  return arg.has_value();
}

TEST(CachedCallbackTest, CanEnqueueCallbacks) {
  base::test::TaskEnvironment task_environment_;
  borealis::StrictCallbackFactory<void(DelayedCache::Result)> callbacks;
  DelayedCache dc;

  dc.Get(callbacks.BindOnce());
  dc.Get(callbacks.BindOnce());
  dc.Get(callbacks.BindOnce());
  // We use a strict mock to show that the below expectation hasn't fired yet
  // and therefore must be queued.
  EXPECT_CALL(callbacks, Call(ExpectedTrue())).Times(3);
  dc.delay = false;
  task_environment_.RunUntilIdle();
}

class NonCompletingCache : public SuccessfulCache {
 public:
  ~NonCompletingCache() override = default;

  // CachedCallback overrides.
  void Build(RealCallback callback) override {
    // Do nothing.
  }
};

TEST(CachedCallbackTest, CanAbort) {
  borealis::StrictCallbackFactory<void(NonCompletingCache::Result)> callbacks;
  auto ncc = std::make_unique<NonCompletingCache>();

  ncc->Get(callbacks.BindOnce());
  ncc->Get(callbacks.BindOnce());
  ncc->Get(callbacks.BindOnce());
  // Because we delete the cache, we expect the callbacks to be invoked with the
  // result of Reject(), which is a default-constructed E unless overridden.
  EXPECT_CALL(callbacks, Call(_))
      .Times(3)
      .WillRepeatedly(Invoke([](NonCompletingCache::Result res) {
        EXPECT_FALSE(res.has_value());
        EXPECT_EQ(res.error(), TestErrors::kFoo);
      }));
  ncc.reset();
}

}  // namespace
}  // namespace guest_os
