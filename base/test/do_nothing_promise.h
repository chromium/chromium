// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TEST_DO_NOTHING_PROMISE_H_
#define BASE_TEST_DO_NOTHING_PROMISE_H_

#include "base/task/promise/no_op_promise_executor.h"

namespace base {

// Creates a promise whose executor doesn't do anything.
struct DoNothingPromiseBuilder {
  explicit DoNothingPromiseBuilder(Location from_here) : from_here(from_here) {}

  const Location from_here;
  bool can_resolve = false;
  bool can_reject = false;
  RejectPolicy reject_policy = RejectPolicy::kMustCatchRejection;

  DoNothingPromiseBuilder& SetCanResolve(bool can_resolve_v) {
    can_resolve = can_resolve_v;
    return *this;
  }

  DoNothingPromiseBuilder& SetCanReject(bool can_reject_v) {
    can_reject = can_reject_v;
    return *this;
  }

  DoNothingPromiseBuilder& SetRejectPolicy(RejectPolicy reject_policy_v) {
    reject_policy = reject_policy_v;
    return *this;
  }

  operator WrappedPromise() const;

  operator scoped_refptr<internal::AbstractPromise>() const;
};

}  // namespace base

#endif  // BASE_TEST_DO_NOTHING_PROMISE_H_
