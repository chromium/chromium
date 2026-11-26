// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/common/operations_controller.h"

#include <ostream>
#include <tuple>

#include "base/check_op.h"
#include "base/synchronization/waitable_event.h"

namespace base::internal {

OperationsController::OperationsController() = default;

OperationsController::~OperationsController() {
#if DCHECK_IS_ON()
  // An OperationsController may only be deleted when it never started
  // accepting operations, or after it was shut down and there are no in-flight
  // attempts to perform operations. After shutting down from
  // kAcceptingOperations, a count of 1 is also expected because the first
  // TryBeginOperation() observing count == 0 leaves +1 permanently in the
  // counter to prevent duplicate shutdown signals.
  auto value = state_and_count_.load();
  DCHECK(
      !(value & kAcceptingOperationsBitMask) ||
      (ExtractState(value) == State::kShuttingDown && ExtractCount(value) <= 1))
      << value;
#endif
}

bool OperationsController::StartAcceptingOperations() {
  // Release semantics are required to ensure that all memory accesses made on
  // this thread happen-before any others done on a thread which is later
  // allowed to perform an operation.
  auto prev_value = state_and_count_.fetch_or(kAcceptingOperationsBitMask,
                                              std::memory_order_release);

  DCHECK_EQ(ExtractState(prev_value), State::kRejectingOperations);
  // The count is the number of rejected operations, unwind them now.
  auto num_rejected = ExtractCount(prev_value);
  if (num_rejected != 0) {
    DecrementBy(num_rejected);
  }
  return num_rejected != 0;
}

OperationsController::OperationToken OperationsController::TryBeginOperation() {
  // Acquire semantics are required to ensure that a thread which is allowed to
  // perform an operation sees all the memory side-effects that happened-before
  // StartAcceptingOperations(). They're also required so that no operations on
  // this thread (e.g. the operation itself) can be reordered before this one.
  auto prev_value = state_and_count_.fetch_add(1, std::memory_order_acquire);

  switch (ExtractState(prev_value)) {
    case State::kRejectingOperations:
      return OperationToken(nullptr);
    case State::kAcceptingOperations:
      return OperationToken(this);
    case State::kShuttingDown:
      // Only decrement if the count was non-zero before our increment. If it
      // was 0, shutdown has already reached zero operations (either
      // ShutdownAndWaitForZeroOperations() observed 0 or an earlier
      // DecrementBy() transitioned 1 -> 0 and owns the sole Signal()). Leaving
      // +1 permanently keeps count >= 1 forever so subsequent rejected attempts
      // cannot transition 1 -> 0 and emit a duplicate Signal().
      if (ExtractCount(prev_value) != 0) {
        DecrementBy(1);
      }
      return OperationToken(nullptr);
  }
}

void OperationsController::ShutdownAndWaitForZeroOperations() {
  // Acquire semantics are required to guarantee that all memory side-effects
  // made by other threads that were allowed to perform operations are
  // synchronized with this thread before it returns from this method.
  auto prev_value = state_and_count_.fetch_or(kShuttingDownBitMask,
                                              std::memory_order_acquire);

  switch (ExtractState(prev_value)) {
    case State::kRejectingOperations:
      // StartAcceptingOperations() was never called, so no OperationTokens
      // exist and shutdown_complete_ will never be waited on. Leave any
      // rejected count in place so subsequent TryBeginOperation() calls never
      // transition the counter to 0 or call Signal().
      break;
    case State::kAcceptingOperations:
      if (ExtractCount(prev_value) != 0) {
        shutdown_complete_.Wait();
        // Acquire the release sequence of all prior DecrementBy() calls so
        // operations that did not call Signal() also happen-before return.
        std::ignore = state_and_count_.load(std::memory_order_acquire);
      }
      break;
    case State::kShuttingDown:
      DCHECK(false) << "Multiple calls to ShutdownAndWaitForZeroOperations()";
      break;
  }
}

// static
OperationsController::State OperationsController::ExtractState(uint32_t value) {
  if (value & kShuttingDownBitMask) {
    return State::kShuttingDown;
  } else if (value & kAcceptingOperationsBitMask) {
    return State::kAcceptingOperations;
  } else {
    return State::kRejectingOperations;
  }
}

void OperationsController::DecrementBy(uint32_t n) {
  DCHECK_GT(n, 0u);
  // Release semantics are required to ensure that no operation on the current
  // thread (e.g. the operation itself) can be reordered after this one.
  auto prev_value = state_and_count_.fetch_sub(n, std::memory_order_release);
  DCHECK_LE(n, ExtractCount(prev_value)) << "Decrement underflow";

  if (ExtractState(prev_value) == State::kShuttingDown &&
      ExtractCount(prev_value) == n) {
    shutdown_complete_.Signal();
  }
}

}  // namespace base::internal
