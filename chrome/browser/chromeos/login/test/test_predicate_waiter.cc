// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/test/test_predicate_waiter.h"

#include "base/callback.h"

namespace chromeos {
namespace test {
namespace {

const base::TimeDelta kPredicateCheckFrequency =
    base::TimeDelta::FromMilliseconds(200);

}  // anonymous namespace

TestPredicateWaiter::TestPredicateWaiter(
    const base::RepeatingCallback<bool(void)>& is_fulfilled)
    : is_fulfilled_(is_fulfilled) {}

TestPredicateWaiter::~TestPredicateWaiter() = default;

void TestPredicateWaiter::Wait() {
  if (is_fulfilled_.Run())
    return;

  timer_.Start(FROM_HERE, kPredicateCheckFrequency, this,
               &TestPredicateWaiter::CheckPredicate);
  run_loop_.Run();
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
}  // namespace chromeos
