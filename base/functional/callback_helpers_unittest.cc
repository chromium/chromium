// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/functional/callback_helpers.h"

#include <functional>
#include <type_traits>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

struct BadArg {};

template <typename TagType, typename CallbackType>
struct TestConversionAndAssignmentImpl {
  static constexpr bool kSupportsConversion =
      std::is_convertible_v<TagType, CallbackType>;
  static constexpr bool kSupportsAssignment =
      std::is_assignable_v<CallbackType, TagType>;
  static_assert(kSupportsConversion == kSupportsAssignment);

  static constexpr bool kValue = kSupportsConversion;
};

template <typename T, typename U>
constexpr bool TestConversionAndAssignment =
    TestConversionAndAssignmentImpl<T, U>::kValue;

#define VOID_RETURN_CALLBACK_TAG_TEST(CallbackType, Sig, BadSig, BoundArg)   \
  static_assert(TestConversionAndAssignment<decltype(base::NullCallback()),  \
                                            CallbackType<Sig>>);             \
  static_assert(                                                             \
      TestConversionAndAssignment<decltype(base::NullCallbackAs<Sig>()),     \
                                  CallbackType<Sig>>);                       \
  static_assert(TestConversionAndAssignment<decltype(base::DoNothing()),     \
                                            CallbackType<Sig>>);             \
  static_assert(                                                             \
      TestConversionAndAssignment<decltype(base::DoNothingAs<Sig>()),        \
                                  CallbackType<Sig>>);                       \
  static_assert(TestConversionAndAssignment<                                 \
                decltype(base::DoNothingWithBoundArgs(BoundArg)),            \
                CallbackType<Sig>>);                                         \
                                                                             \
  static_assert(                                                             \
      !TestConversionAndAssignment<decltype(base::NullCallbackAs<BadSig>()), \
                                   CallbackType<Sig>>);                      \
  static_assert(                                                             \
      !TestConversionAndAssignment<decltype(base::DoNothingAs<BadSig>()),    \
                                   CallbackType<Sig>>);                      \
  static_assert(TestConversionAndAssignment<                                 \
                decltype(base::DoNothingWithBoundArgs(BadArg())),            \
                CallbackType<Sig>>)

#define NON_VOID_RETURN_CALLBACK_TAG_TEST(CallbackType, Sig, BadSig, BoundArg) \
  static_assert(TestConversionAndAssignment<decltype(base::NullCallback()),    \
                                            CallbackType<Sig>>);               \
  static_assert(                                                               \
      TestConversionAndAssignment<decltype(base::NullCallbackAs<Sig>()),       \
                                  CallbackType<Sig>>);                         \
                                                                               \
  /* Unlike callbacks that return void, callbacks that return non-void      */ \
  /* should not be implicitly convertible from DoNothingCallbackTag since   */ \
  /* this would require guessing what the callback should return.           */ \
  static_assert(!TestConversionAndAssignment<decltype(base::DoNothing()),      \
                                             CallbackType<Sig>>);              \
  static_assert(                                                               \
      !TestConversionAndAssignment<decltype(base::DoNothingAs<Sig>()),         \
                                   CallbackType<Sig>>);                        \
  static_assert(!TestConversionAndAssignment<                                  \
                decltype(base::DoNothingWithBoundArgs(BoundArg)),              \
                CallbackType<Sig>>);                                           \
                                                                               \
  static_assert(                                                               \
      !TestConversionAndAssignment<decltype(base::NullCallbackAs<BadSig>()),   \
                                   CallbackType<Sig>>);                        \
  static_assert(                                                               \
      !TestConversionAndAssignment<decltype(base::DoNothingAs<BadSig>()),      \
                                   CallbackType<Sig>>);                        \
  static_assert(!TestConversionAndAssignment<                                  \
                decltype(base::DoNothingWithBoundArgs(BadArg())),              \
                CallbackType<Sig>>)

VOID_RETURN_CALLBACK_TAG_TEST(base::OnceCallback, void(), void(char), );
VOID_RETURN_CALLBACK_TAG_TEST(base::OnceCallback, void(int), void(char), 8);
NON_VOID_RETURN_CALLBACK_TAG_TEST(base::OnceCallback, int(int), char(int), 8);

VOID_RETURN_CALLBACK_TAG_TEST(base::RepeatingCallback, void(), void(char), );
VOID_RETURN_CALLBACK_TAG_TEST(base::RepeatingCallback,
                              void(int),
                              void(char),
                              8);
NON_VOID_RETURN_CALLBACK_TAG_TEST(base::RepeatingCallback,
                                  int(int),
                                  char(int),
                                  8);

#undef VOID_RETURN_CALLBACK_TAG_TEST
#undef NON_VOID_RETURN_CALLBACK_TAG_TEST

TEST(CallbackHelpersTest, IsBaseCallback) {
  // Check that base::{Once,Repeating}Closures and references to them are
  // considered base::{Once,Repeating}Callbacks.
  static_assert(base::IsBaseCallback<base::OnceClosure>);
  static_assert(base::IsBaseCallback<base::RepeatingClosure>);
  static_assert(base::IsBaseCallback<base::OnceClosure&&>);
  static_assert(base::IsBaseCallback<const base::RepeatingClosure&>);

  // Check that base::{Once, Repeating}Callbacks with a given RunType and
  // references to them are considered base::{Once, Repeating}Callbacks.
  static_assert(base::IsBaseCallback<base::OnceCallback<int(int)>>);
  static_assert(base::IsBaseCallback<base::RepeatingCallback<int(int)>>);
  static_assert(base::IsBaseCallback<base::OnceCallback<int(int)>&&>);
  static_assert(base::IsBaseCallback<const base::RepeatingCallback<int(int)>&>);

  // Check that POD types are not considered base::{Once, Repeating}Callbacks.
  static_assert(!base::IsBaseCallback<bool>);
  static_assert(!base::IsBaseCallback<int>);
  static_assert(!base::IsBaseCallback<double>);

  // Check that the closely related std::function is not considered a
  // base::{Once, Repeating}Callback.
  static_assert(!base::IsBaseCallback<std::function<void()>>);
  static_assert(!base::IsBaseCallback<const std::function<void()>&>);
  static_assert(!base::IsBaseCallback<std::function<void()>&&>);
}

void Increment(int* value) {
  (*value)++;
}

void IncrementWithRef(int& value) {
  value++;
}

int IncrementAndReturn(int* value) {
  return ++(*value);
}

TEST(CallbackHelpersTest, ScopedClosureRunnerHasClosure) {
  base::ScopedClosureRunner runner1;
  EXPECT_FALSE(runner1);

  base::ScopedClosureRunner runner2{base::DoNothing()};
  EXPECT_TRUE(runner2);
}

TEST(CallbackHelpersTest, ScopedClosureRunnerExitScope) {
  int run_count = 0;
  {
    base::ScopedClosureRunner runner(base::BindOnce(&Increment, &run_count));
    EXPECT_EQ(0, run_count);
  }
  EXPECT_EQ(1, run_count);
}

TEST(CallbackHelpersTest, ScopedClosureRunnerRelease) {
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

TEST(CallbackHelpersTest, ScopedClosureRunnerReplaceClosure) {
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

TEST(CallbackHelpersTest, ScopedClosureRunnerRunAndResetNonNull) {
  int run_count_3 = 0;
  {
    base::ScopedClosureRunner runner(base::BindOnce(&Increment, &run_count_3));
    EXPECT_EQ(0, run_count_3);
    runner.RunAndReset();
    EXPECT_EQ(1, run_count_3);
  }
  EXPECT_EQ(1, run_count_3);
}

TEST(CallbackHelpersTest, ScopedClosureRunnerRunAndResetNull) {
  base::ScopedClosureRunner runner;
  runner.RunAndReset();  // Should not crash.
}

TEST(CallbackHelpersTest, ScopedClosureRunnerMoveConstructor) {
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

TEST(CallbackHelpersTest, ScopedClosureRunnerMoveAssignment) {
  int run_count_1 = 0;
  int run_count_2 = 0;
  {
    base::ScopedClosureRunner runner(base::BindOnce(&Increment, &run_count_1));
    {
      base::ScopedClosureRunner runner2(
          base::BindOnce(&Increment, &run_count_2));
      runner = std::move(runner2);
      EXPECT_EQ(1, run_count_1);
      EXPECT_EQ(0, run_count_2);
    }
    EXPECT_EQ(1, run_count_1);
    EXPECT_EQ(0, run_count_2);
  }
  EXPECT_EQ(1, run_count_1);
  EXPECT_EQ(1, run_count_2);
}

TEST(CallbackHelpersTest, SplitOnceCallback_EmptyCallback) {
  base::OnceCallback<void(int*)> cb = base::NullCallback();
  EXPECT_FALSE(cb);

  auto split = base::SplitOnceCallback(std::move(cb));

  static_assert(std::is_same_v<decltype(split),
                               std::pair<base::OnceCallback<void(int*)>,
                                         base::OnceCallback<void(int*)>>>,
                "");
  EXPECT_FALSE(split.first);
  EXPECT_FALSE(split.second);
}

TEST(CallbackHelpersTest, SplitOnceCallback_FirstCallback) {
  int count = 0;
  base::OnceCallback<void(int*)> cb =
      base::BindOnce([](int* count) { ++*count; });

  auto split = base::SplitOnceCallback(std::move(cb));

  static_assert(std::is_same_v<decltype(split),
                               std::pair<base::OnceCallback<void(int*)>,
                                         base::OnceCallback<void(int*)>>>,
                "");

  EXPECT_EQ(0, count);
  std::move(split.first).Run(&count);
  EXPECT_EQ(1, count);

#if GTEST_HAS_DEATH_TEST
  EXPECT_CHECK_DEATH(std::move(split.second).Run(&count));
#endif  // GTEST_HAS_DEATH_TEST
}

TEST(CallbackHelpersTest, SplitOnceCallback_SecondCallback) {
  int count = 0;
  base::OnceCallback<void(int*)> cb =
      base::BindOnce([](int* count) { ++*count; });

  auto split = base::SplitOnceCallback(std::move(cb));

  static_assert(std::is_same_v<decltype(split),
                               std::pair<base::OnceCallback<void(int*)>,
                                         base::OnceCallback<void(int*)>>>,
                "");

  EXPECT_EQ(0, count);
  std::move(split.second).Run(&count);
  EXPECT_EQ(1, count);

  EXPECT_CHECK_DEATH(std::move(split.first).Run(&count));
}

TEST(CallbackHelpersTest, SplitSplitOnceCallback_FirstSplit) {
  int count = 0;
  base::OnceCallback<void(int*)> cb =
      base::BindOnce([](int* count) { ++*count; });

  auto split = base::SplitOnceCallback(std::move(cb));
  base::OnceCallback<void(int*)> cb1 = std::move(split.first);
  split = base::SplitOnceCallback(std::move(split.second));
  base::OnceCallback<void(int*)> cb2 = std::move(split.first);
  base::OnceCallback<void(int*)> cb3 = std::move(split.second);

  EXPECT_EQ(0, count);
  std::move(cb1).Run(&count);
  EXPECT_EQ(1, count);

  EXPECT_CHECK_DEATH(std::move(cb3).Run(&count));
}

TEST(CallbackHelpersTest, SplitSplitOnceCallback_SecondSplit) {
  int count = 0;
  base::OnceCallback<void(int*)> cb =
      base::BindOnce([](int* count) { ++*count; });

  auto split = base::SplitOnceCallback(std::move(cb));
  base::OnceCallback<void(int*)> cb1 = std::move(split.first);
  split = base::SplitOnceCallback(std::move(split.second));
  base::OnceCallback<void(int*)> cb2 = std::move(split.first);
  base::OnceCallback<void(int*)> cb3 = std::move(split.second);

  EXPECT_EQ(0, count);
  std::move(cb2).Run(&count);
  EXPECT_EQ(1, count);

  EXPECT_CHECK_DEATH(std::move(cb1).Run(&count));
}

TEST(CallbackHelpersTest, IgnoreArgs) {
  int count = 0;
  base::RepeatingClosure repeating_closure =
      base::BindRepeating(&Increment, &count);
  base::OnceClosure once_closure = base::BindOnce(&Increment, &count);

  base::RepeatingCallback<void(int)> repeating_int_cb =
      base::IgnoreArgs<int>(repeating_closure);
  EXPECT_EQ(0, count);
  repeating_int_cb.Run(42);
  EXPECT_EQ(1, count);
  repeating_int_cb.Run(42);
  EXPECT_EQ(2, count);

  base::OnceCallback<void(int)> once_int_cb =
      base::IgnoreArgs<int>(std::move(once_closure));
  EXPECT_EQ(2, count);
  std::move(once_int_cb).Run(42);
  EXPECT_EQ(3, count);

  // Ignore only some (one) argument and forward the rest.
  auto repeating_callback = base::BindRepeating(&Increment);
  auto repeating_cb_with_extra_arg = base::IgnoreArgs<bool>(repeating_callback);
  repeating_cb_with_extra_arg.Run(false, &count);
  EXPECT_EQ(4, count);

  // Ignore two arguments and forward the rest.
  auto once_callback = base::BindOnce(&Increment);
  auto once_cb_with_extra_arg =
      base::IgnoreArgs<char, bool>(repeating_callback);
  std::move(once_cb_with_extra_arg).Run('d', false, &count);
  EXPECT_EQ(5, count);
}

TEST(CallbackHelpersTest, IgnoreArgs_EmptyCallback) {
  base::RepeatingCallback<void(int)> repeating_int_cb =
      base::IgnoreArgs<int>(base::RepeatingClosure());
  EXPECT_FALSE(repeating_int_cb);

  base::OnceCallback<void(int)> once_int_cb =
      base::IgnoreArgs<int>(base::OnceClosure());
  EXPECT_FALSE(once_int_cb);
}

TEST(CallbackHelpersTest, IgnoreArgs_NonVoidReturn) {
  int count = 0;
  base::RepeatingCallback<int(void)> repeating_no_param_cb =
      base::BindRepeating(&IncrementAndReturn, &count);
  base::OnceCallback<int(void)> once_no_param_cb =
      base::BindOnce(&IncrementAndReturn, &count);

  base::RepeatingCallback<int(int)> repeating_int_cb =
      base::IgnoreArgs<int>(repeating_no_param_cb);
  EXPECT_EQ(0, count);
  EXPECT_EQ(1, repeating_int_cb.Run(42));
  EXPECT_EQ(1, count);
  EXPECT_EQ(2, repeating_int_cb.Run(42));
  EXPECT_EQ(2, count);

  base::OnceCallback<int(int)> once_int_cb =
      base::IgnoreArgs<int>(std::move(once_no_param_cb));
  EXPECT_EQ(2, count);
  EXPECT_EQ(3, std::move(once_int_cb).Run(42));
  EXPECT_EQ(3, count);

  // Ignore only some (one) argument and forward the rest.
  auto repeating_cb = base::BindRepeating(&IncrementAndReturn);
  auto repeating_cb_with_extra_arg = base::IgnoreArgs<bool>(repeating_cb);
  EXPECT_EQ(4, repeating_cb_with_extra_arg.Run(false, &count));
  EXPECT_EQ(4, count);

  // Ignore two arguments and forward the rest.
  auto once_cb = base::BindOnce(&IncrementAndReturn);
  auto once_cb_with_extra_arg =
      base::IgnoreArgs<char, bool>(std::move(once_cb));
  EXPECT_EQ(5, std::move(once_cb_with_extra_arg).Run('d', false, &count));
  EXPECT_EQ(5, count);
}

TEST(CallbackHelpersTest, ForwardRepeatingCallbacks) {
  int count = 0;
  auto tie_cb =
      base::ForwardRepeatingCallbacks({base::BindRepeating(&IncrementWithRef),
                                       base::BindRepeating(&IncrementWithRef)});

  tie_cb.Run(count);
  EXPECT_EQ(count, 2);

  tie_cb.Run(count);
  EXPECT_EQ(count, 4);
}

TEST(CallbackHelpersTest, ReturnValueOnce) {
  // Check that copyable types are supported.
  auto string_factory = base::ReturnValueOnce(std::string("test"));
  static_assert(std::is_same_v<decltype(string_factory),
                               base::OnceCallback<std::string(void)>>);
  EXPECT_EQ(std::move(string_factory).Run(), "test");

  // Check that move-only types are supported.
  auto unique_ptr_factory = base::ReturnValueOnce(std::make_unique<int>(42));
  static_assert(std::is_same_v<decltype(unique_ptr_factory),
                               base::OnceCallback<std::unique_ptr<int>(void)>>);
  EXPECT_EQ(*std::move(unique_ptr_factory).Run(), 42);
}

}  // namespace
