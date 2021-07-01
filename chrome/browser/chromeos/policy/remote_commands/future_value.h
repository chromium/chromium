// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_FUTURE_VALUE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_FUTURE_VALUE_H_

#include <memory>
#include <string>

#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/time/time.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace policy {

// Representation of a value that will be populated asynchronously.
// Provides accessors to wait for the value to arrive.
//
// This makes it very easy to unittest a method that asynchronously reports its
// result by calling a callback.
//
// If your test calls future_value.SetValue(...) from inside the callback,
// your main test code can wait for this value using future_value.Wait()
// or future_value.Get().
//
// GetWithTimeout()/WaitWithTimeout() are alternative methods which behave much
// nicer than Get()/Wait() if (when) you break the test.
// While Get()/Wait() will hang for a very long time and make it hard to figure
// out what test failed (let alone where it failed), the WithTimeout versions
// will:
//    - fail much faster (by default after 5 seconds).
//    - provide a stacktrace that clearly shows the test name and call location.
//    - allow a custom message to be printed.
//
// Example usage:
//
//  class MyTestFixture {
//   protected:
//    // Callback invoked by the code under test when it's ready.
//    // This will unblock the main test thread by calling
//    // |future_result_.SetValue()|.
//    void OnJobFinished(ResultType value) { future_result_.SetValue(value); }
//
//    FutureValue<ResultType> future_result_;
//  };
//
//   TEST_F(MyTestFixture, MyTest) {
//     //... snip
//     object_under_test.DoSomethingAsync(
//         base::BindOnce(&MyTestFixture::OnJobFinished,
//             base::Unretained(this)));
//
//    ResultType& actual_result = future_result_.GetWithTimeout();
//
//    // When you come here, DoSomethingAsync has finished and |actual_result|
//    // contains the result passed to the callback.
//  }
//

// Default error message printed when using FutureValue::WaitWithTimeout().
constexpr char kFutureValueDefaultTimeoutMessage[] =
    "Timeout waiting for future value";
// Default timeout when using FutureValue::WaitWithTimeout().
constexpr int kFutureValueDefaultTimeoutInSeconds = 5;

template <typename Type>
class FutureValue {
 public:
  FutureValue() = default;
  FutureValue(const FutureValue&) = delete;
  FutureValue& operator=(const FutureValue&) = delete;
  FutureValue(FutureValue&&) = delete;
  FutureValue& operator=(FutureValue&&) = delete;
  ~FutureValue() = default;

  // Wait for the value to arrive, and return the result.
  const Type& Get() {
    Wait();
    return value();
  }

  // Wait for the value to arrive.
  // Will wait forever if SetValue() is never called.
  void Wait() {
    WaitWithTimeout(/*error_message=*/"", base::TimeDelta::FiniteMax());
  }

  // Set the value of the future.
  // This will unblock any pending Wait() or Get() call.
  void SetValue(Type value) {
    value_ = std::move(value);
    if (run_loop_)
      run_loop_->Quit();
  }

  // Get the current value, or DCHECK if no value is set.
  const Type& value() const {
    DCHECK(has_value());
    return value_.value();
  }

  Type& mutable_value() {
    DCHECK(has_value());
    return value_.value();
  }

  bool has_value() const { return value_.has_value(); }

  // Unset the current value, so this object can be used again for a new value.
  void Reset() {
    value_.reset();
    run_loop_.reset();
  }

  // Wait for the value to arrive.
  // Will timeout and fail the test if SetValue() isn't called within the
  // specified `timeout`.
  // If a timeout happens, the specified `error_message` will be reported.
  void WaitWithTimeout(const std::string& error_message =
                           std::string(kFutureValueDefaultTimeoutMessage),
                       base::TimeDelta timeout = base::TimeDelta::FromSeconds(
                           kFutureValueDefaultTimeoutInSeconds)) {
    if (!value_) {
      base::test::ScopedRunLoopTimeout run_loop_timeout(
          FROM_HERE, timeout, base::BindLambdaForTesting([error_message]() {
            return error_message;
          }));

      run_loop_ = std::make_unique<base::RunLoop>();
      run_loop_->Run();
    }
  }

  const Type& GetWithTimeout(
      const std::string& error_message =
          std::string(kFutureValueDefaultTimeoutMessage),
      base::TimeDelta timeout =
          base::TimeDelta::FromSeconds(kFutureValueDefaultTimeoutInSeconds)) {
    WaitWithTimeout(error_message, timeout);
    return value();
  }

 private:
  std::unique_ptr<base::RunLoop> run_loop_;
  absl::optional<Type> value_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_REMOTE_COMMANDS_FUTURE_VALUE_H_
