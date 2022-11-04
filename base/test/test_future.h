// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_FUTURE_H_
#define BASE_TEST_TEST_FUTURE_H_

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/callback_forward.h"
#include "base/check.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/test_future_internal.h"
#include "base/thread_annotations.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base::test {

// Helper class to test code that returns its result(s) asynchronously through a
// callback:
//
//    - Pass the callback provided by TestFuture::GetCallback() to the code
//      under test.
//    - Wait for the callback to be invoked by calling TestFuture::Wait(), or
//      TestFuture::Get() to access the value(s) passed to the callback.
//
// If the callback takes multiple arguments, use TestFuture::Get<0>() to access
// the value of the first argument, TestFuture::Get<1>() to access the value of
// the second argument, and so on.
// Alternatively you can use the argument type like TestFuture::Get<T>().
//
// If for any reason you can't use TestFuture::GetCallback(), you can use
// TestFuture::SetValue() to directly set the value. This method must be called
// from the main sequence.
//
// Finally, TestFuture::Take() is similar to TestFuture::Get() but it will
// move the result out, which can be helpful when testing a move-only class.
//
// Example usage:
//
//   TEST_F(MyTestFixture, MyTest) {
//     TestFuture<ResultType> future;
//
//     object_under_test.DoSomethingAsync(future.GetCallback());
//
//     const ResultType& actual_result = future.Get();
//
//     // When you come here, DoSomethingAsync has finished and `actual_result`
//     // contains the result passed to the callback.
//   }
//
// Example if the callback has 2 arguments:
//
//   TEST_F(MyTestFixture, MyTest) {
//     TestFuture<int, std::string> future;
//
//     object_under_test.DoSomethingAsync(future.GetCallback());
//
//     // Either select the argument by type...
//     int first_argument = future.Get<int>();
//     const std::string& second_argument = future.Get<std::string>();
//
//     // ... or by index.
//     int first_argument = future.Get<0>();
//     const std::string& second_argument = future.Get<1>();
//   }
//
// Example if the callback has zero arguments:
//
//   TEST_F(MyTestFixture, MyTest) {
//     TestFuture<void> signal;
//
//     object_under_test.DoSomethingAsync(signal.GetCallback());
//
//     EXPECT_TRUE(signal.Wait());
//     // When you come here you know the async code is ready.
//   }
//
// Or an example using TestFuture::Wait():
//
//   TEST_F(MyTestFixture, MyWaitTest) {
//     TestFuture<ResultType> future;
//
//     object_under_test.DoSomethingAsync(future.GetCallback());
//
//     // Optional. The Get() call below will also wait until the value
//     // arrives, but this explicit call to Wait() can be useful if you want to
//     // add extra information.
//     ASSERT_TRUE(future.Wait()) << "Detailed error message";
//
//     const ResultType& actual_result = future.Get();
//   }
//
// All access to this class must be made from the same sequence.
template <typename... Types>
class TestFuture {
 public:
  using TupleType = std::tuple<std::decay_t<Types>...>;

  static_assert(std::tuple_size<TupleType>::value > 0,
                "Don't use TestFuture<> but use TestFuture<void> instead");

  TestFuture() = default;
  TestFuture(const TestFuture&) = delete;
  TestFuture& operator=(const TestFuture&) = delete;
  ~TestFuture() = default;

  // Waits for the value to arrive.
  //
  // Returns true if the value arrived, or false if a timeout happens.
  //
  // Directly calling Wait() is not required as Get()/Take() will also wait for
  // the value to arrive, however you can use a direct call to Wait() to
  // improve the error reported:
  //
  //   ASSERT_TRUE(queue.Wait()) << "Detailed error message";
  //
  [[nodiscard]] bool Wait() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (values_)
      return true;

    run_loop_.Run();

    return IsReady();
  }

  // Returns true if the value has arrived.
  bool IsReady() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return values_.has_value();
  }

  // Waits for the value to arrive, and returns the I-th value.
  //
  // Will DCHECK if a timeout happens.
  //
  // Example usage:
  //
  //   TestFuture<int, std::string> future;
  //   int first = future.Get<0>();
  //   std::string second = future.Get<1>();
  //
  template <std::size_t I,
            typename T = TupleType,
            internal::EnableIfOneOrMoreValues<T> = true>
  const auto& Get() {
    return std::get<I>(GetTuple());
  }

  // Waits for the value to arrive, and returns the value with the given type.
  //
  // Will DCHECK if a timeout happens.
  //
  // Example usage:
  //
  //   TestFuture<int, std::string> future;
  //   int first = future.Get<int>();
  //   std::string second = future.Get<std::string>();
  //
  template <typename Type>
  const auto& Get() {
    return std::get<Type>(GetTuple());
  }

  // Returns a callback that when invoked will store all the argument values,
  // and unblock any waiters.
  // Templated so you can specify how you need the arguments to be passed -
  // const, reference, .... Defaults to simply `Types...`.
  //
  // Example usage:
  //
  //   TestFuture<int, std::string> future;
  //
  //   // returns base::OnceCallback<void(int, std::string)>
  //   future.GetCallback();
  //
  //   // returns base::OnceCallback<void(int, const std::string&)>
  //   future.GetCallback<int, const std::string&>();
  //
  template <typename... CallbackArgumentsTypes>
  base::OnceCallback<void(CallbackArgumentsTypes...)> GetCallback() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::BindOnce(
        [](WeakPtr<TestFuture<Types...>> future,
           CallbackArgumentsTypes... values) {
          if (future)
            future->SetValue(std::forward<CallbackArgumentsTypes>(values)...);
        },
        weak_ptr_factory_.GetWeakPtr());
  }

  base::OnceCallback<void(Types...)> GetCallback() {
    return GetCallback<Types...>();
  }

  // Sets the value of the future.
  // This will unblock any pending Wait() or Get() call.
  // This can only be called once.
  void SetValue(Types... values) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    DCHECK(!values_.has_value())
        << "The value of a TestFuture can only be set once. If you need to "
           "handle an ordered stream of result values, use "
           "`base::test::RepeatingTestFuture`.";

    values_ = std::make_tuple(std::forward<Types>(values)...);
    run_loop_.Quit();
  }

  //////////////////////////////////////////////////////////////////////////////
  //  Accessor methods only available if the future holds a single value.
  //////////////////////////////////////////////////////////////////////////////

  // Waits for the value to arrive, and returns its value.
  //
  // Will DCHECK if a timeout happens.
  template <typename T = TupleType, internal::EnableIfSingleValue<T> = true>
  [[nodiscard]] const auto& Get() {
    return std::get<0>(GetTuple());
  }

  // Waits for the value to arrive, and move it out.
  //
  // Will DCHECK if a timeout happens.
  template <typename T = TupleType, internal::EnableIfSingleValue<T> = true>
  [[nodiscard]] auto Take() {
    return std::get<0>(TakeTuple());
  }

  //////////////////////////////////////////////////////////////////////////////
  //  Accessor methods only available if the future holds multiple values.
  //////////////////////////////////////////////////////////////////////////////

  // Waits for the values to arrive, and returns a tuple with the values.
  //
  // Will DCHECK if a timeout happens.
  template <typename T = TupleType, internal::EnableIfMultiValue<T> = true>
  [[nodiscard]] const TupleType& Get() {
    return GetTuple();
  }

  // Waits for the values to arrive, and move a tuple with the values out.
  //
  // Will DCHECK if a timeout happens.
  template <typename T = TupleType, internal::EnableIfMultiValue<T> = true>
  [[nodiscard]] TupleType Take() {
    return TakeTuple();
  }

 private:
  [[nodiscard]] const TupleType& GetTuple() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool success = Wait();
    DCHECK(success) << "Waiting for value timed out.";
    return values_.value();
  }

  [[nodiscard]] TupleType TakeTuple() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool success = Wait();
    DCHECK(success) << "Waiting for value timed out.";
    return std::move(values_.value());
  }

  SEQUENCE_CHECKER(sequence_checker_);

  base::RunLoop run_loop_ GUARDED_BY_CONTEXT(sequence_checker_);

  absl::optional<TupleType> values_ GUARDED_BY_CONTEXT(sequence_checker_);

  base::WeakPtrFactory<TestFuture<Types...>> weak_ptr_factory_{this};
};

// Specialization so you can use `TestFuture` to wait for a no-args callback.
//
// This specialization offers a subset of the methods provided on the base
// `TestFuture`, as there is no value to be returned.
template <>
class TestFuture<void> {
 public:
  // Waits until the callback or `SetValue()` is invoked.
  //
  // Fails your test if a timeout happens, but you can check the return value
  // to improve the error reported:
  //
  //   ASSERT_TRUE(future.Wait()) << "Detailed error message";
  [[nodiscard]] bool Wait() { return implementation_.Wait(); }

  // Waits until the callback or `SetValue()` is invoked.
  void Get() { std::ignore = implementation_.Get(); }

  // Returns true if the callback or `SetValue()` was invoked.
  bool IsReady() const { return implementation_.IsReady(); }

  // Returns a callback that when invoked will unblock any waiters.
  base::OnceCallback<void()> GetCallback() {
    return base::BindOnce(implementation_.GetCallback(), true);
  }

  // Indicates this `TestFuture` is ready, and unblocks any waiters.
  void SetValue() { implementation_.SetValue(true); }

 private:
  TestFuture<bool> implementation_;
};

}  // namespace base::test

#endif  // BASE_TEST_TEST_FUTURE_H_
