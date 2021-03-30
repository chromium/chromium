// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_LOGIN_TEST_TEST_CONDITION_WAITER_H_
#define CHROME_BROWSER_ASH_LOGIN_TEST_TEST_CONDITION_WAITER_H_

#include "base/macros.h"

namespace chromeos {
namespace test {

// Generic class for conditions that can be awaited it test.
class TestConditionWaiter {
 public:
  virtual ~TestConditionWaiter() = default;
  virtual void Wait() = 0;

 protected:
  TestConditionWaiter() = default;

  DISALLOW_COPY_AND_ASSIGN(TestConditionWaiter);
};

}  // namespace test
}  // namespace chromeos

#endif  // CHROME_BROWSER_ASH_LOGIN_TEST_TEST_CONDITION_WAITER_H_
