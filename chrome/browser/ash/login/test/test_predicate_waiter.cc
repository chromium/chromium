// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/test/test_predicate_waiter.h"

#include "base/functional/callback.h"
#include "base/logging.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace test {
namespace {

const base::TimeDelta kPredicateCheckFrequency = base::Milliseconds(200);

}  // anonymous namespace

TestPredicateWaiter::TestPredicateWaiter(
    const base::RepeatingCallback<bool(void)>& is_fulfilled)
    : is_fulfilled_(is_fulfilled) {}

TestPredicateWaiter::~TestPredicateWaiter() = default;

void TestPredicateWaiter::Wait() {
  if (is_fulfilled_.Run())
    return;

  if (!description_.empty()) {
    LOG(INFO) << "Actually waiting for " << description_;
  }

  timer_.Start(FROM_HERE, kPredicateCheckFrequency, this,
               &TestPredicateWaiter::CheckPredicate);
  run_loop_.Run();
  ASSERT_TRUE(is_fulfilled_.Run());
}

void TestPredicateWaiter::CheckPredicate() {
  if (is_fulfilled_.Run()) {
    run_loop_.Quit();
    timer_.Stop();
  } else {
    timer_.Reset();
  }
}

}  // namespace test
}  // namespace ash
