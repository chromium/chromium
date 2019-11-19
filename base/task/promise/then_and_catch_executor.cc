// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/promise/then_and_catch_executor.h"

namespace base {
namespace internal {

bool ThenAndCatchExecutorCommon::IsCancelled() const {
  if (!then_callback_.is_null()) {
    // If there is both a resolve and a reject executor they must be canceled
    // at the same time.
    DCHECK(catch_callback_.is_null() ||
           catch_callback_.IsCancelled() == then_callback_.IsCancelled());
    return then_callback_.IsCancelled();
  }
  return catch_callback_.IsCancelled();
}

void ThenAndCatchExecutorCommon::Execute(AbstractPromise* promise,
                                         ExecuteCallback execute_then,
                                         ExecuteCallback execute_catch) {
  AbstractPromise* prerequisite = promise->GetOnlyPrerequisite();
  if (prerequisite->IsResolved()) {
    if (ProcessNullCallback(then_callback_, prerequisite, promise))
      return;

    execute_then(prerequisite, promise, &then_callback_);
  } else {
    DCHECK(prerequisite->IsRejected());
    if (ProcessNullCallback(catch_callback_, prerequisite, promise))
      return;

    execute_catch(prerequisite, promise, &catch_callback_);
  }
}

// static
bool ThenAndCatchExecutorCommon::ProcessNullCallback(
    const CallbackBase& callback,
    AbstractPromise* arg,
    AbstractPromise* result) {
  if (callback.is_null()) {
    // A curried promise is used to forward the result through null callbacks.
    result->emplace(scoped_refptr<AbstractPromise>(arg));
    DCHECK(result->IsResolvedWithPromise());
    return true;
  }

  return false;
}

}  // namespace internal
}  // namespace base
