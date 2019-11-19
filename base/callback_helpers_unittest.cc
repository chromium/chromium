// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback_helpers.h"

#include <functional>

#include "base/bind.h"
#include "base/callback.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(CallbackHelpersTest, IsBaseCallback) {
  // Check that base::{Once,Repeating}Closures and references to them are
  // considered base::{Once,Repeating}Callbacks.
  static_assert(base::IsBaseCallback<base::OnceClosure>::value, "");
  static_assert(base::IsBaseCallback<base::RepeatingClosure>::value, "");
  static_assert(base::IsBaseCallback<base::OnceClosure&&>::value, "");
  static_assert(base::IsBaseCallback<const base::RepeatingClosure&>::value, "");

  // Check that base::Callbacks with a given RunType and references to them are
  // considered base::Callbacks.
  static_assert(base::IsBaseCallback<base::OnceCallback<int(int)>>::value, "");
  static_assert(base::IsBaseCallback<base::RepeatingCallback<int(int)>>::value,
                "");
  static_assert(base::IsBaseCallback<base::OnceCallback<int(int)>&&>::value,
                "");
  static_assert(
      base::IsBaseCallback<const base::RepeatingCallback<int(int)>&>::value,
      "");

  // Check that POD types are not considered base::Callbacks.
  static_assert(!base::IsBaseCallback<bool>::value, "");
  static_assert(!base::IsBaseCallback<int>::value, "");
  static_assert(!base::IsBaseCallback<double>::value, "");

  // Check that the closely related std::function is not considered a
  // base::Callback.
  static_assert(!base::IsBaseCallback<std::function<void()>>::value, "");
  static_assert(!base::IsBaseCallback<const std::function<void()>&>::value, "");
  static_assert(!base::IsBaseCallback<std::function<void()>&&>::value, "");
}

void Increment(int* value) {
  (*value)++;
}

TEST(CallbackHelpersTest, TestScopedClosureRunnerExitScope) {
  int run_count = 0;
  {
    base::ScopedClosureRunner runner(base::BindOnce(&Increment, &run_count));
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(CallbackHelpersTest, TestScopedClosureRunnerRelease) {
  int run_count = 0;
  base::OnceClosure c;
  {
    base::ScopedClosureRunner runner(base::BindOnce(&Increment, &run_count));
    c = runner.Release();
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(0, run_count);
  std::move(c).Run();
  EXPECT_EQ(1, run_count);
}

TEST(CallbackHelpersTest, TestScopedClosureRunnerReplaceClosure) {
  int run_count_1 = 0;
  int run_count_2 = 0;
  {
    base::ScopedClosureRunner runner;
    runner.ReplaceClosure(base::BindOnce(&Increment, &run_count_1));
    runner.ReplaceClosure(base::BindOnce(&Increment, &run_count_2));
    EXPECT_EQ(0, run_count_1);
    EXPECT_EQ(0, run_count_2);
  }
  EXPECT_EQ(0, run_count_1);
  EXPECT_EQ(1, run_count_2);
}

TEST(CallbackHelpersTest, TestScopedClosureRunnerRunAndReset) {
  int run_count_3 = 0;
  {
    base::ScopedClosureRunner runner(base::BindOnce(&Increment, &run_count_3));
    EXPECT_EQ(0, run_count_3);
    runner.RunAndReset();
    EXPECT_EQ(1, run_count_3);
  }
  EXPECT_EQ(1, run_count_3);
}

TEST(CallbackHelpersTest, TestScopedClosureRunnerMoveConstructor) {
  int run_count = 0;
  {
    std::unique_ptr<base::ScopedClosureRunner> runner(
        new base::ScopedClosureRunner(base::BindOnce(&Increment, &run_count)));
    base::ScopedClosureRunner runner2(std::move(*runner));
    runner.reset();
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(CallbackHelpersTest, TestScopedClosureRunnerMoveAssignment) {
  int run_count_1 = 0;
  int run_count_2 = 0;
  {
    base::ScopedClosureRunner runner(base::BindOnce(&Increment, &run_count_1));
    {
      base::ScopedClosureRunner runner2(
          base::BindOnce(&Increment, &run_count_2));
      runner = std::move(runner2);
      EXPECT_EQ(0, run_count_1);
      EXPECT_EQ(0, run_count_2);
    }
    EXPECT_EQ(0, run_count_1);
    EXPECT_EQ(0, run_count_2);
  }
  EXPECT_EQ(0, run_count_1);
  EXPECT_EQ(1, run_count_2);
}

TEST(CallbackHelpersTest, TestAdaptCallbackForRepeating) {
  int count = 0;
  base::OnceCallback<void(int*)> cb =
      base::BindOnce([](int* count) { ++*count; });

  base::RepeatingCallback<void(int*)> wrapped =
      base::AdaptCallbackForRepeating(std::move(cb));

  EXPECT_EQ(0, count);
  wrapped.Run(&count);
  EXPECT_EQ(1, count);
  wrapped.Run(&count);
  EXPECT_EQ(1, count);
}

}  // namespace
