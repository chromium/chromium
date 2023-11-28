// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/composite_waiter.h"

#include <memory>
#include <utility>

#include "chrome/browser/ash/login/test/test_condition_waiter.h"

namespace ash::test {

CompositeWaiter::CompositeWaiter(std::unique_ptr<TestConditionWaiter> first,
                                 std::unique_ptr<TestConditionWaiter> second)
    : first_(std::move(first)), second_(std::move(second)) {}

CompositeWaiter::CompositeWaiter(std::unique_ptr<TestConditionWaiter> first,
                                 std::unique_ptr<TestConditionWaiter> second,
                                 std::unique_ptr<TestConditionWaiter> third)
    : first_(std::move(first)),
      second_(std::move(second)),
      third_(std::move(third)) {}

CompositeWaiter::~CompositeWaiter() = default;

void CompositeWaiter::Wait() {
  first_->Wait();
  first_.reset();
  second_->Wait();
  if (!third_) {
    return;
  }
  second_.reset();
  third_->Wait();
}

}  // namespace ash::test
