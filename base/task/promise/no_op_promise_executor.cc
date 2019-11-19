// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/no_op_promise_executor.h"
#include "base/task_runner.h"

namespace base {
namespace internal {

NoOpPromiseExecutor::NoOpPromiseExecutor(bool can_resolve, bool can_reject)
#if DCHECK_IS_ON()
    : can_resolve_(can_resolve),
      can_reject_(can_reject)
#endif
{
}

NoOpPromiseExecutor::~NoOpPromiseExecutor() {}

bool NoOpPromiseExecutor::IsCancelled() const {
  return false;
}

#if DCHECK_IS_ON()
PromiseExecutor::ArgumentPassingType
NoOpPromiseExecutor::ResolveArgumentPassingType() const {
  return PromiseExecutor::ArgumentPassingType::kNoCallback;
}

PromiseExecutor::ArgumentPassingType
NoOpPromiseExecutor::RejectArgumentPassingType() const {
  return PromiseExecutor::ArgumentPassingType::kNoCallback;
}

bool NoOpPromiseExecutor::CanResolve() const {
  return can_resolve_;
}

bool NoOpPromiseExecutor::CanReject() const {
  return can_reject_;
}
#endif

void NoOpPromiseExecutor::Execute(AbstractPromise* promise) {}

// static
PassedPromise NoOpPromiseExecutor::Create(Location from_here,
                                          bool can_resolve,
                                          bool can_reject,
                                          RejectPolicy reject_policy) {
  return PassedPromise(AbstractPromise::CreateNoPrerequisitePromise(
      from_here, reject_policy, DependentList::ConstructUnresolved(),
      PromiseExecutor::Data(in_place_type_t<NoOpPromiseExecutor>(), can_resolve,
                            can_reject)));
}

}  // namespace internal
}  // namespace base
