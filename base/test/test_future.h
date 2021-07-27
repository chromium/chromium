// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_TEST_FUTURE_H_
#define BASE_TEST_TEST_FUTURE_H_

#include <memory>
#include <string>

#include "base/callback_forward.h"
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/bind.h"
#include "base/thread_annotations.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace test {

namespace internal {

// Helper to only implement a method if the future holds a single value
template <typename Tuple>
using EnableIfSingleValue =
    std::enable_if_t<(std::tuple_size<Tuple>::value <= 1), bool>;

// Helper to only implement a method if the future holds multiple values
template <typename Tuple>
using EnableIfMultiValue =
    std::enable_if_t<(std::tuple_size<Tuple>::value > 1), bool>;

}  // namespace internal

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
//
// If for any reason you can't use TestFuture::GetCallback(), you can use
// TestFuture::SetValue() to directly set the value. This method must be called
// from the main sequence.
//
// A |base::test::ScopedRunLoopTimeout| can be used to control how long
// TestFuture::Get() and TestFuture::Wait() block before timing out.
// In case of a timeout:
//     - TestFuture::Wait() will return 'false'.
//     - TestFuture::Get() will DCHECK.
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
//     // When you come here, DoSomethingAsync has finished and |actual_result|
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
//     int first_argument = future.Get<0>();
//     const std::string & second_argument = future.Get<1>();
//   }
//
// Or an example using TestFuture::Wait():
//
//   TEST_F(MyTestFixture, MyWaitTest) {
//     TestFuture<ResultType> future;
//
//     object_under_test.DoSomethingAsync(future.GetCallback());
//
//     bool success = future.Wait();
//
//     // Optional. If a timeout happened, the test will already be in a failed
//     // state, but an explicit check can be useful if you want to add extra
//     // information.
//     ASSERT_TRUE(success) << "Detailed error message";
//
//     const ResultType& actual_result = future.Get();
//   }
//
// All access to this class must be made from the same thread.
template <typename... Types>
class TestFuture {
 public:
  // Helper type to make the SFINAE templates easier on the eyes.
  using T = std::tuple<Types...>;
  using FirstType = typename std::tuple_element<0, T>::type;

  TestFuture() = default;
  TestFuture(const TestFuture&) = delete;
  TestFuture& operator=(const TestFuture&) = delete;
  ~TestFuture() = default;

  // Wait for the value to arrive.
  //
  // Returns true if the value arrives, or false if a timeout happens.
  // A timeout can only happen if |base::test::ScopedRunLoopTimeout| is used in
  // the calling context. In case of a timeout, the test will be failed
  // automatically by |base::test::ScopedRunLoopTimeout|, however if you want to
  // provide a better error message you can always add an explicit check:
  //
  //   ASSERT_TRUE(future.Wait()) << "Detailed error message";
  //
  bool Wait() {
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

  // Wait for the value to arrive, and return the I-th value.
  //
  // Will DCHECK if a timeout happens.
  //
  // Example usage:
  //
  //   TestFutureTuple<int, std::string> future;
  //   int first = future.Get<0>();
  //   std::string second = future.Get<1>();
  //
  template <std::size_t I>
  const typename std::tuple_element<I, std::tuple<Types...>>::type& Get() {
    return std::get<I>(GetTuple());
  }

  // Returns a callback that when invoked will store all the argument values,
  // and unblock any waiters.
  // Templated so you can specify how you need the arguments to be passed -
  // const, reference, .... Defaults to simply |Types...|.
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
    return base::BindOnce(&TestFuture<Types...>::SetValueFromCallbackArguments<
                              CallbackArgumentsTypes...>,
                          weak_ptr_factory_.GetWeakPtr());
  }

  base::OnceCallback<void(Types...)> GetCallback() {
    return GetCallback<Types...>();
  }

  // Set the value of the future.
  // This will unblock any pending Wait() or Get() call.
  // This can only be called once.
  void SetValue(Types... values) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    DCHECK(!values_.has_value())
        << "The value of a TestFuture can only be set once.";

    values_ = std::make_tuple(std::forward<Types>(values)...);
    run_loop_.Quit();
  }

  //////////////////////////////////////////////////////////////////////////////
  //  Accessor methods only available if the future holds a single value.
  //////////////////////////////////////////////////////////////////////////////

  // Wait for the value to arrive, and returns its value.
  //
  // Will DCHECK if a timeout happens.
  template <typename U = T, internal::EnableIfSingleValue<U> = true>
  const FirstType& Get() WARN_UNUSED_RESULT {
    return std::get<0>(GetTuple());
  }

  // Wait for the value to arrive, and move it out.
  //
  // Will DCHECK if a timeout happens.
  template <typename U = T, internal::EnableIfSingleValue<U> = true>
  FirstType Take() WARN_UNUSED_RESULT {
    return std::get<0>(TakeTuple());
  }

  //////////////////////////////////////////////////////////////////////////////
  //  Accessor methods only available if the future holds multiple values.
  //////////////////////////////////////////////////////////////////////////////

  // Wait for the values to arrive, and returns a tuple with the values.
  //
  // Will DCHECK if a timeout happens.
  template <typename U = T, internal::EnableIfMultiValue<U> = true>
  const std::tuple<Types...>& Get() WARN_UNUSED_RESULT {
    return GetTuple();
  }

  // Wait for the values to arrive, and move a tuple with the values out.
  //
  // Will DCHECK if a timeout happens.
  template <typename U = T, internal::EnableIfMultiValue<U> = true>
  std::tuple<Types...> Take() WARN_UNUSED_RESULT {
    return TakeTuple();
  }

 private:
  // Used by GetCallback() to adapt between the form in which the callback
  // provides arguments, and the argument types specified to this template.
  // e.g. callbacks often carry arguments as |const Foo&| rather than |Foo|.
  template <typename... CallbackArgumentsTypes>
  void SetValueFromCallbackArguments(CallbackArgumentsTypes... values) {
    SetValue(std::forward<CallbackArgumentsTypes>(values)...);
  }

  const std::tuple<Types...>& GetTuple() WARN_UNUSED_RESULT {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool success = Wait();
    DCHECK(success) << "Waiting for value timed out.";
    return values_.value();
  }

  std::tuple<Types...> TakeTuple() WARN_UNUSED_RESULT {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    bool success = Wait();
    DCHECK(success) << "Waiting for value timed out.";
    return std::move(values_.value());
  }

  SEQUENCE_CHECKER(sequence_checker_);

  base::RunLoop run_loop_ GUARDED_BY_CONTEXT(sequence_checker_);

  absl::optional<std::tuple<Types...>> values_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Task runner this class is tied to.
  // All methods must be called from this same sequence.
  scoped_refptr<base::SequencedTaskRunner> task_runner_ =
      base::SequencedTaskRunnerHandle::Get();

  base::WeakPtrFactory<TestFuture<Types...>> weak_ptr_factory_{this};
};

}  // namespace test
}  // namespace base

#endif  // BASE_TEST_TEST_FUTURE_H_
