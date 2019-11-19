// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/promise_executor.h"

namespace base {
namespace internal {

PromiseExecutor::~PromiseExecutor() {
  if (data_.vtable_)
    data_.vtable_->destructor(data_.storage_.array);
  data_.vtable_ = nullptr;
}

#if DCHECK_IS_ON()
PromiseExecutor::ArgumentPassingType
PromiseExecutor::ResolveArgumentPassingType() const {
  return data_.vtable_->resolve_argument_passing_type(data_.storage_.array);
}

PromiseExecutor::ArgumentPassingType
PromiseExecutor::RejectArgumentPassingType() const {
  return data_.vtable_->reject_argument_passing_type(data_.storage_.array);
}

bool PromiseExecutor::CanResolve() const {
  return data_.vtable_->can_resolve(data_.storage_.array);
}

bool PromiseExecutor::CanReject() const {
  return data_.vtable_->can_reject(data_.storage_.array);
}
#endif

}  // namespace internal
}  // namespace base
