// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_COMPOSITE_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_COMPOSITE_WAITER_H_

#include "base/functional/callback_forward.h"
#include "base/run_loop.h"
#include "base/timer/timer.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"

namespace ash::test {

// Sequentially waits several events.
class CompositeWaiter : public TestConditionWaiter {
 public:
  CompositeWaiter(std::unique_ptr<TestConditionWaiter> first,
                  std::unique_ptr<TestConditionWaiter> second);

  CompositeWaiter(std::unique_ptr<TestConditionWaiter> first,
                  std::unique_ptr<TestConditionWaiter> second,
                  std::unique_ptr<TestConditionWaiter> third);

  CompositeWaiter(const CompositeWaiter&) = delete;
  CompositeWaiter& operator=(const CompositeWaiter&) = delete;

  ~CompositeWaiter() override;

  // TestConditionWaiter
  void Wait() override;

 private:
  std::unique_ptr<TestConditionWaiter> first_;
  std::unique_ptr<TestConditionWaiter> second_;
  std::unique_ptr<TestConditionWaiter> third_;
};

}  // namespace ash::test

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_COMPOSITE_WAITER_H_
