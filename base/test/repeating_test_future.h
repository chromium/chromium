// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_REPEATING_TEST_FUTURE_H_
#define BASE_TEST_REPEATING_TEST_FUTURE_H_

#include <utility>

#include "base/check.h"
#include "base/containers/queue.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker.h"
#include "base/test/test_future_internal.h"
#include "base/thread_annotations.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base::test {

// Related class to `base::test::TestFuture`, which allows its callback and
// AddValue() method to be called multiple times.
//
// Each call to Take() will return one element in FIFO fashion.
// If no element is available, Take() will wait until an element becomes
// available.
//
// Just like `base::test::TestFuture`, `base::test::RepeatingTestFuture` also
// supports callbacks which take multiple values. If this is the case Take()
// will return a tuple containing all values passed to the callback.
//
// Example usage:
//
//   TEST_F(MyTestFixture, MyTest) {
//     RepeatingTestFuture<ResultType> future;
//
//     InvokeCallbackAsyncTwice(future.GetCallback());
//
//     ResultType first_result = future.Take();
//     ResultType second_result = future.Take();
//
//     // When you come here, InvokeCallbackAsyncTwice has finished,
//     // `first_result` contains the value passed to the first invocation
//     // of the callback, and `second_result` has the result of the second
//     // invocation.
//   }
//
// Example without using a callback but using AddValue() instead:
//
//   TEST_F(MyTestFixture, MyTest) {
//     RepeatingTestFuture<std::string> future;
//
//     // AddValue() can be used to add an element to the future.
//     future.AddValue("first-value");
//     future.AddValue("second-value");
//
//     EXPECT_EQ("first-value", future.Take());
//     EXPECT_EQ("second-value", future.Take());
//   }
//
// Or an example using RepeatingTestFuture::Wait():
//
//   TEST_F(MyTestFixture, MyWaitTest) {
//     RepeatingTestFuture<ResultType> future;
//
//     object_under_test.DoSomethingAsync(future.GetCallback());
//
//     // Optional. The Take() call below will also wait until the value
//     // arrives, but this explicit call to Wait() can be useful if you want to
//     // add extra information.
//     ASSERT_TRUE(future.Wait()) << "Detailed error message";
//
//     ResultType actual_result = future.Take();
//   }
//
// All access to this class must be made from the same sequence.
template <typename... Types>
class RepeatingTestFuture {
 public:
  using TupleType = std::tuple<std::decay_t<Types>...>;

  RepeatingTestFuture() = default;
  RepeatingTestFuture(const RepeatingTestFuture&) = delete;
  RepeatingTestFuture& operator=(const RepeatingTestFuture&) = delete;
  RepeatingTestFuture(RepeatingTestFuture&&) = delete;
  RepeatingTestFuture& operator=(RepeatingTestFuture&&) = delete;
  ~RepeatingTestFuture() = default;

  void AddValue(Types... values) {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    elements_.push(std::make_tuple(std::forward<Types>(values)...));
    SignalElementIsAvailable();
  }

  // Waits until an element is available.
  // Returns immediately if one or more elements are already available.
  //
  // Returns true if an element arrived, or false if a timeout happens.
  //
  // Directly calling Wait() is not required as Take() will also wait for
  // the value to arrive, however you can use a direct call to Wait() to
  // improve the error reported:
  //
  //   ASSERT_TRUE(queue.Wait()) << "Detailed error message";
  //
  [[nodiscard]] bool Wait() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    if (IsEmpty())
      WaitForANewElement();

    return !IsEmpty();
  }

  // Returns a callback that when invoked will store all the argument values,
  // and unblock any waiters.
  // This method is templated so you can specify how you need the arguments to
  // be passed - be it const, as reference, or anything you can think off.
  // By default the callback accepts the arguments as `Types...`.
  //
  // Example usage:
  //
  //   RepeatingTestFuture<int, std::string> future;
  //
  //   // returns base::RepeatingCallback<void(int, std::string)>
  //   future.GetCallback();
  //
  //   // returns base::RepeatingCallback<void(int, const std::string&)>
  //   future.GetCallback<int, const std::string&>();
  //
  template <typename... CallbackArgumentsTypes>
  base::RepeatingCallback<void(CallbackArgumentsTypes...)> GetCallback() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return base::BindRepeating(
        [](WeakPtr<RepeatingTestFuture<Types...>> future,
           CallbackArgumentsTypes... values) {
          if (future)
            future->AddValue(std::forward<CallbackArgumentsTypes>(values)...);
        },
        weak_ptr_factory_.GetWeakPtr());
  }

  base::RepeatingCallback<void(Types...)> GetCallback() {
    return GetCallback<Types...>();
  }

  // Returns true if no elements are currently present. Note that consuming all
  // elements through Take() will cause this method to return true after the
  // last available element has been consumed.
  bool IsEmpty() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    return elements_.empty();
  }

  //////////////////////////////////////////////////////////////////////////////
  //  Accessor methods only available if each element in the future holds a
  //  single value.
  //////////////////////////////////////////////////////////////////////////////

  // Wait for an element to arrive, and move its value out.
  //
  // Will DCHECK if a timeout happens.
  template <typename T = TupleType, internal::EnableIfSingleValue<T> = true>
  auto Take() {
    return std::get<0>(TakeTuple());
  }

  //////////////////////////////////////////////////////////////////////////////
  //  Accessor methods only available if each element in the future holds
  //  multiple values.
  //////////////////////////////////////////////////////////////////////////////

  // Wait for an element to arrive, and move a tuple with its values out.
  //
  // Will DCHECK if a timeout happens.
  template <typename T = TupleType, internal::EnableIfMultiValue<T> = true>
  TupleType Take() {
    return TakeTuple();
  }

 private:
  // Wait until a new element is available.
  void WaitForANewElement() VALID_CONTEXT_REQUIRED(sequence_checker_) {
    DCHECK(!run_loop_.has_value());

    // Create a new run loop.
    run_loop_.emplace();
    // Wait until 'run_loop_->Quit()' is called.
    run_loop_->Run();
    run_loop_.reset();
  }

  void SignalElementIsAvailable() VALID_CONTEXT_REQUIRED(sequence_checker_) {
    if (run_loop_.has_value())
      run_loop_->Quit();
  }

  TupleType TakeTuple() {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

    // Ensure an element is available.
    bool success = Wait();
    DCHECK(success) << "Waiting for an element timed out.";

    auto result = std::move(elements_.front());
    elements_.pop();
    return result;
  }

  base::queue<TupleType> elements_ GUARDED_BY_CONTEXT(sequence_checker_);

  // Used by Wait() to know when AddValue() is called.
  absl::optional<base::RunLoop> run_loop_ GUARDED_BY_CONTEXT(sequence_checker_);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<RepeatingTestFuture<Types...>> weak_ptr_factory_{this};
};

}  // namespace base::test

#endif  // BASE_TEST_REPEATING_TEST_FUTURE_H_
